//
// Created by fydeos on 23-3-21.
//

#include <map>
#include <emscripten.h>
#include <rime_api.h>
#include <emscripten/wasmfs.h>
#include <emscripten/bind.h>
#include <unistd.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>
#include <malloc.h>

void memoryStatistics() {
  unsigned int totalMemory = EM_ASM_INT(return HEAP8.length);
  printf("Total memory: %u bytes\n", totalMemory);
  struct mallinfo i = mallinfo();
  unsigned int dynamicTop = (unsigned int)sbrk(0);
  int free_mem = totalMemory - dynamicTop + i.fordblks;
  printf("Free memory: %u bytes\n", free_mem);
  printf("Used: %u bytes\n", totalMemory - free_mem);
}

void rime_require_module_lua();

namespace wasmfs_rime {
backend_t my_wasmfs_create_fast_indexeddb_backend();
}

static RimeApi* api;

static void WasmRimeFinialize() {
  api->finalize();
}

#include <boost/throw_exception.hpp>

void boost::throw_exception(std::exception const& e) {
  fprintf(stderr, "%s\n", e.what());
  abort();
}

void boost::throw_exception(std::exception const& e,
  boost::source_location const&) {
  fprintf(stderr, "%s\n", e.what());
  abort();
}

using namespace emscripten;

// Get char index from byte index in a utf8 string
static int ConvertUtf8Index(const char* str, int b_idx) {
  int c;
  int cur_idx = 0;
  int chr_idx = 0;
  while (cur_idx < b_idx) {
    c = str[cur_idx];
    if ((c & 0x80) == 0) {
      cur_idx += 1;
    } else if ((c & 0xE0) == 0xC0) {
      cur_idx += 2;
    } else if ((c & 0xF0) == 0xE0) {
      cur_idx += 3;
    } else if ((c & 0xF8) == 0xF0) {
      cur_idx += 4;
    }
    chr_idx++;
  }
  return chr_idx;
}

struct CRimeSession : boost::noncopyable,
                      std::enable_shared_from_this<CRimeSession> {
  static std::map<RimeSessionId, CRimeSession*> sessionMap;

  RimeSessionId sessionId;
  val onOptionChangedFunction;
  CRimeSession() { sessionId = 0; }

  void Initialize(std::string schema_id,
    std::string schema_config,
    val onOptionChanged) {
    if (!sessionId) {
      LOG(WARNING) << "Creating session";
      sessionId = api->create_session();
      Bool b = api->select_schema_with_config(sessionId,
        schema_id.c_str(),
        schema_config.c_str());
      if (!b) {
        LOG(ERROR) << "Failed to create new session";
        api->destroy_session(sessionId);
      }
    }
    onOptionChangedFunction = onOptionChanged;
    sessionMap.insert({ sessionId, this });
  }

  ~CRimeSession() {
    if (sessionId) {
      api->destroy_session(sessionId);
      sessionMap.erase(sessionId);
    }
  }

  bool ProcessKey(int key_id, int mask) {
    return api->process_key(sessionId, key_id, mask);
  }

  std::string Hello() { return "Hello"; }

  val GetContext() {
    RIME_STRUCT(RimeContext, context);
    bool got = api->get_context(sessionId, &context);
    if (!got) {
      return val::null();
    }
    auto v = val::object();

    v.set("composition", val::object());
    v["composition"].set("length", context.composition.length);
    v["composition"].set("cursorPosition",
      ConvertUtf8Index(context.composition.preedit,
        context.composition.cursor_pos));
    v["composition"].set("selectionStart",
      ConvertUtf8Index(context.composition.preedit,
        context.composition.sel_start));
    v["composition"].set("selectionEnd",
      ConvertUtf8Index(context.composition.preedit,
        context.composition.sel_end));
    v["composition"].set("preedit",
      val::u8string(context.composition.preedit));

    v.set("menu", val::object());
    v["menu"].set("pageSize", context.menu.page_size);
    v["menu"].set("pageNumber", context.menu.page_no);
    v["menu"].set("isLastPage", (bool)context.menu.is_last_page);
    v["menu"].set("highlightedCandidateIndex",
      context.menu.highlighted_candidate_index);
    v["menu"].set("candidates", val::array());
    for (int i = 0; i < context.menu.num_candidates; i++) {
      v["menu"]["candidates"].set(i, val::object());
      v["menu"]["candidates"][i].set("text",
        val::u8string(context.menu.candidates[i].text));
      v["menu"]["candidates"][i].set("comment",
        val::u8string(context.menu.candidates[i].comment));
    }
    v["menu"].set("selectKeys", val::u8string(context.menu.select_keys));

    v.set("commitTextPreview", val::u8string(context.commit_text_preview));
    v.set("selectLabels", val::array());
    for (int i = 0; i < context.menu.page_size; i++) {
      v["selectLabels"].set(i, val::u8string(context.select_labels[i]));
    }

    api->free_context(&context);
    return v;
  }

  val GetCommit() {
    RIME_STRUCT(RimeCommit, commit);
    bool got = api->get_commit(sessionId, &commit);
    if (!got) {
      return val::null();
    }
    auto v = val::object();
    v.set("text", val::u8string(commit.text));

    api->free_commit(&commit);
    return v;
  }

  val GetStatus() {
    RIME_STRUCT(RimeStatus, status);
    bool got = api->get_status(sessionId, &status);
    if (!got) {
      return val::null();
    }
    auto v = val::object();
    v.set("schemaId", val::u8string(status.schema_id));
    v.set("schemaName", val::u8string(status.schema_name));
    v.set("isDisabled", bool(status.is_disabled));
    v.set("isComposing", bool(status.is_composing));
    v.set("isAsciiMode", bool(status.is_ascii_mode));
    v.set("isFullShape", bool(status.is_full_shape));
    v.set("isSimplified", bool(status.is_simplified));
    v.set("isTraditional", bool(status.is_traditional));
    v.set("isAsciiPunct", bool(status.is_ascii_punct));
    api->free_status(&status);
    return v;
  }

  void ClearComposition() { api->clear_composition(sessionId); }

  val GetCurrentSchema() {
    char bbb[30];
    if (api->get_current_schema(sessionId, bbb, sizeof(bbb))) {
      return val::u8string(bbb);
    }
    return val::null();
  }

  bool ActionCandidate(int id, int op, bool currentPage) {
    if (op == 0) {
      if (currentPage) {
        return api->select_candidate_on_current_page(sessionId, id);
      } else {
        return api->select_candidate(sessionId, id);
      }
    } else if (op == 1) {
      if (currentPage) {
        return api->delete_candidate_on_current_page(sessionId, id);
      } else {
        return api->delete_candidate(sessionId, id);
      }
    }
    return false;
  }

  bool GetOption(std::string name) {
    bool val = api->get_option(sessionId, name.c_str());
    return val;
  }

  void SetOption(std::string name, bool val) {
    bool oldVal = api->get_option(sessionId, name.c_str());
    // Only set option if value doesn't match, to prevent dead loop
    if (oldVal != val) {
      api->set_option(sessionId, name.c_str(), val);
    }
  }

  std::string GetOptionLabel(std::string name, bool state) {
    return api->get_state_label(sessionId, name.c_str(), state);
  }


  struct CandidateIterator : boost::noncopyable {
    std::shared_ptr<CRimeSession> session;
    RimeCandidateListIterator iter;
    bool ok;
    CandidateIterator(std::shared_ptr<CRimeSession> session, int idx)
        : session(session) {
      memset(&iter, 0, sizeof(iter));
      ok = RimeCandidateListFromIndex(session->sessionId, &iter, idx);
    }

    void Advance() {
      if (ok) {
        ok = RimeCandidateListNext(&iter);
      }
    }

    val Current() {
      if (ok) {
        auto v = val::object();
        v.set("index", iter.index);
        v.set("text", std::string(iter.candidate.text));
        v.set("comment", std::string(iter.candidate.comment));
        return v;
      } else {
        return val::null();
      }
    }

    ~CandidateIterator() { RimeCandidateListEnd(&iter); }
  };

  std::shared_ptr<CandidateIterator> IterateCandidates(int idx) {
    return std::make_shared<CandidateIterator>(shared_from_this(), idx);
  }
};

std::map<RimeSessionId, CRimeSession*> CRimeSession::sessionMap;

static void on_message(void* context_object,
  RimeSessionId session_id,
  const char* message_type,
  const char* message_value) {
  printf("message: [%lu] [%s] %s\n",
    session_id,
    message_type,
    message_value);
  if (strcmp(message_type, "option") == 0) {
    Bool state = message_value[0] != '!';
    const char* option_name = message_value + !state;
    auto ptr = CRimeSession::sessionMap.find(session_id);
    if (ptr != CRimeSession::sessionMap.end()) {
      ptr->second->onOptionChangedFunction(std::string(option_name), state);
    }
  }
}

static void WasmRimeSetup() {
  rime_require_module_lua();
  api = rime_get_api();
  printf("WASM compiled at: %s %s\n", __DATE__, __TIME__);
  printf("Initializing...\n");
  backend_t backend = wasmfs_rime::my_wasmfs_create_fast_indexeddb_backend();
  wasmfs_create_directory("/working", 0777, backend);
  chdir("/working");

  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = "/working/shared";
  traits.user_data_dir = "/working/user";
  traits.prebuilt_data_dir = "/working/build";
  traits.staging_dir = "/working/build";
  traits.app_name = "rime.wasm";
  api->setup(&traits);
  api->set_notification_handler(&on_message, NULL);
  api->initialize(&traits);

  printf("Rime setup done\n");
}

val WasmRimeGetSchemaList() {
  RimeSchemaList list;
  if (!api->get_schema_list(&list)) {
    return val::null();
  }
  auto v = val::array();
  for (int i = 0; i < list.size; i++) {
    auto o = val::object();
    o.set("id", val::u8string(list.list[i].schema_id));
    o.set("name", val::u8string(list.list[i].name));
    v.set(i, o);
  }
  api->free_schema_list(&list);
  return v;
}

void WasmRimeRebuildPrismForSchema(std::string schema_id,
  std::string schema_config) {
  api->rebuild_prism_for_schema(schema_id.c_str(), schema_config.c_str());
}

void PerformMaintenance(bool full) {
  // Multithreading has been removed, so this operation will complete synchronously
  api->start_maintenance(full);
}


EMSCRIPTEN_BINDINGS(WasmRime) {
  function("rimeSetup", &WasmRimeSetup);
  function("rimeFinalize", &WasmRimeFinialize);
  function("rimePerformMaintenance", &PerformMaintenance);
  function("rimeGetSchemaList", &WasmRimeGetSchemaList);
  function("rimeRebuildPrismForSchema", &WasmRimeRebuildPrismForSchema);
  class_<CRimeSession>("RimeSession")
    .smart_ptr_constructor("RimeSession", &std::make_shared<CRimeSession>)
    .function("initialize", &CRimeSession::Initialize)
    .function("hello", &CRimeSession::Hello)
    .function("processKey", &CRimeSession::ProcessKey)
    .function("getContext", &CRimeSession::GetContext)
    .function("getStatus", &CRimeSession::GetStatus)
    .function("getCommit", &CRimeSession::GetCommit)
    .function("clearComposition", &CRimeSession::ClearComposition)
    .function("getCurrentSchema", &CRimeSession::GetCurrentSchema)
    .function("actionCandidate", &CRimeSession::ActionCandidate)
    .function("getOption", &CRimeSession::GetOption)
    .function("setOption", &CRimeSession::SetOption)
    .function("getOptionLabel", &CRimeSession::GetOptionLabel)
    .function("iterateCandidates", &CRimeSession::IterateCandidates);
  class_<CRimeSession::CandidateIterator>("CandidateIterator")
    .smart_ptr<std::shared_ptr<CRimeSession::CandidateIterator>>(
      "CandidateIterator")
    .function("advance", &CRimeSession::CandidateIterator::Advance)
    .function("current", &CRimeSession::CandidateIterator::Current);
}