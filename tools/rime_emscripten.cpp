//
// Created by fydeos on 23-3-21.
//


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


namespace wasmfs_rime {
  backend_t my_wasmfs_create_fast_indexeddb_backend(const char* root);
}

static void on_message(void* context_object,
                       RimeSessionId session_id,
                       const char* message_type,
                       const char* message_value) {
  printf("message: [%lu] [%s] %s\n", session_id, message_type, message_value);
  RimeApi* rime = rime_get_api();
  if (RIME_API_AVAILABLE(rime, get_state_label) &&
      !strcmp(message_type, "option")) {
    Bool state = message_value[0] != '!';
    const char* option_name = message_value + !state;
    const char* state_label =
        rime->get_state_label(session_id, option_name, state);
    if (state_label) {
      printf("updated option: %s = %d // %s\n", option_name, state, state_label);
    }
  }
}

static void WasmRimeSetup() {
  printf("WASM compiled at: %s %s\n", __DATE__, __TIME__);
  printf("Initializing...\n");
  backend_t backend = wasmfs_rime::my_wasmfs_create_fast_indexeddb_backend("rime-data");
  wasmfs_create_directory("/working", 0777, backend);
  chdir("/working");

  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = "/working";
  traits.user_data_dir = "/working";
  traits.app_name = "rime.wasm";
  RimeSetup(&traits);
  RimeSetNotificationHandler(&on_message, NULL);
  RimeInitialize(&traits);

  printf("Rime setup done\n");
}

static void WasmRimeFinialize() {
  RimeFinalize();
}

#include <boost/throw_exception.hpp>

void boost::throw_exception(std::exception const & e){
  fprintf(stderr, "%s\n", e.what());
  abort();
}

void boost::throw_exception(std::exception const & e, boost::source_location const&){
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

struct CRimeSession : boost::noncopyable, std::enable_shared_from_this<CRimeSession> {
  RimeSessionId sessionId;
  CRimeSession() {
    sessionId = 0;
  }

  void Initialize() {
    if (!sessionId) {
      LOG(WARNING) << "Creating session";
      sessionId = RimeCreateSession();
    }
  }

  ~CRimeSession() {
    if (sessionId) {
      RimeDestroySession(sessionId);
    }
  }

  bool ProcessKey(int key_id, int mask) {
    return RimeProcessKey(sessionId, key_id, mask);
  }

  std::string Hello() {
    return "Hello";
  }

  val GetContext() {
    RIME_STRUCT(RimeContext, context);
    bool got = RimeGetContext(sessionId, &context);
    if (!got) {
      return val::null();
    }
    auto v = val::object();

    v.set("composition", val::object());
    v["composition"].set("length",context.composition.length);
    v["composition"].set("cursorPosition", ConvertUtf8Index(context.composition.preedit, context.composition.cursor_pos));
    v["composition"].set("selectionStart", ConvertUtf8Index(context.composition.preedit, context.composition.sel_start));
    v["composition"].set("selectionEnd", ConvertUtf8Index(context.composition.preedit, context.composition.sel_end));
    v["composition"].set("preedit", val::u8string(context.composition.preedit));

    v.set("menu", val::object());
    v["menu"].set("pageSize", context.menu.page_size);
    v["menu"].set("pageNumber", context.menu.page_no);
    v["menu"].set("isLastPage", (bool)context.menu.is_last_page);
    v["menu"].set("highlightedCandidateIndex", context.menu.highlighted_candidate_index);
    v["menu"].set("candidates", val::array());
    for (int i = 0; i < context.menu.num_candidates; i++) {
      v["menu"]["candidates"].set(i, val::object());
      v["menu"]["candidates"][i].set("text", val::u8string(context.menu.candidates[i].text));
      v["menu"]["candidates"][i].set("comment", val::u8string(context.menu.candidates[i].comment));
    }
    v["menu"].set("selectKeys", val::u8string(context.menu.select_keys));

    v.set("commitTextPreview", val::u8string(context.commit_text_preview));
    v.set("selectLabels", val::array());
    for (int i = 0; i < context.menu.page_size; i++) {
      v["selectLabels"].set(i, val::u8string(context.select_labels[i]));
    }

    RimeFreeContext(&context);
    return v;
  }

  val GetCommit() {
    RIME_STRUCT(RimeCommit, commit);
    bool got = RimeGetCommit(sessionId, &commit);
    if (!got) {
      return val::null();
    }
    auto v =  val::object();
    v.set("text", val::u8string(commit.text));

    RimeFreeCommit(&commit);
    return v;
  }

  val GetStatus() {
    RIME_STRUCT(RimeStatus, status);
    bool got = RimeGetStatus(sessionId, &status);
    if (!got) {
      return val::null();
    }
    auto v =  val::object();
    v.set("schemaId", val::u8string(status.schema_id));
    v.set("schemaName", val::u8string(status.schema_name));
    v.set("isDisabled", bool(status.is_disabled));
    v.set("isComposing", bool(status.is_composing));
    v.set("isAsciiMode", bool(status.is_ascii_mode));
    v.set("isFullShape", bool(status.is_full_shape));
    v.set("isSimplified", bool(status.is_simplified));
    v.set("isTraditional", bool(status.is_traditional));
    v.set("isAsciiPunct", bool(status.is_ascii_punct));
    RimeFreeStatus(&status);
    return v;
  }

  void ClearComposition() {
    RimeClearComposition(sessionId);
  }

  val GetCurrentSchema() {
    char bbb[30];
    if (RimeGetCurrentSchema(sessionId, bbb, sizeof(bbb))) {
      return val::u8string(bbb);
    }
    return val::null();
  }

  bool SelectSchema(std::string schema_id) {
    return RimeSelectSchema(sessionId, schema_id.c_str());
  }
};

val WasmRimeGetSchemaList() {
  RimeSchemaList list;
  if (!RimeGetSchemaList(&list)) {
    return val::null();
  }
  auto v = val::array();
  for (int i = 0; i < list.size; i++) {
    auto o = val::object();
    o.set("id", val::u8string(list.list[i].schema_id));
    o.set("name", val::u8string(list.list[i].name));
    v.set(i, o);
  }
  RimeFreeSchemaList(&list);
  return v;
}

void PerformMaintenance(bool full) {
  // Multithreading has been removed, so this operation will complete synchronously
  RimeStartMaintenance(full);
}

EMSCRIPTEN_BINDINGS(WasmRime) {
  function("rimeSetup", &WasmRimeSetup);
  function("rimeFinalize", &WasmRimeFinialize);
  function("rimePerformMaintenance", &PerformMaintenance);
  function("rimeGetSchemaList", &WasmRimeGetSchemaList);
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
      .function("selectSchema", &CRimeSession::SelectSchema)
      ;
}