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

struct CRimeSession : boost::noncopyable, std::enable_shared_from_this<CRimeSession> {
  RimeSessionId sessionId;
  CRimeSession() { }

  void Initialize() {
    if (!sessionId) {
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
    v["composition"].set("cursor_pos", context.composition.cursor_pos);
    v["composition"].set("sel_start", context.composition.sel_start);
    v["composition"].set("sel_end", context.composition.sel_end);
    v["composition"].set("preedit", val::u8string(context.composition.preedit));

    v.set("menu", val::object());
    v["menu"].set("page_size", context.menu.page_size);
    v["menu"].set("page_no", context.menu.page_no);
    v["menu"].set("is_last_page", (bool)context.menu.is_last_page);
    v["menu"].set("highlighted_candidate_index", (bool)context.menu.highlighted_candidate_index);
    v["menu"].set("candidates", val::array());
    for (int i = 0; i < context.menu.num_candidates; i++) {
      v["menu"]["candidates"].set(i, val::object());
      v["menu"]["candidates"][i].set("text", val::u8string(context.menu.candidates[i].text));
      v["menu"]["candidates"][i].set("comment", val::u8string(context.menu.candidates[i].comment));
    }
    v["menu"].set("select_keys", val::u8string(context.menu.select_keys));

    v.set("commit_text_preview", val::u8string(context.commit_text_preview));
    v.set("select_labels", val::array());
    for (int i = 0; i < context.menu.page_size; i++) {
      v["select_labels"].set(i, val::u8string(context.select_labels[i]));
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
    v.set("schema_id", val::u8string(status.schema_id));
    v.set("schema_name", val::u8string(status.schema_name));
    v.set("is_disabled", bool(status.is_disabled));
    v.set("is_composing", bool(status.is_composing));
    v.set("is_ascii_mode", bool(status.is_ascii_mode));
    v.set("is_full_shape", bool(status.is_full_shape));
    v.set("is_simplified", bool(status.is_simplified));
    v.set("is_traditional", bool(status.is_traditional));
    v.set("is_ascii_punct", bool(status.is_ascii_punct));
    RimeFreeStatus(&status);
    return v;
  }
};

EMSCRIPTEN_BINDINGS(WasmRime) {
  function("rimeSetup", &WasmRimeSetup);
  class_<CRimeSession>("RimeSession")
      .smart_ptr_constructor("RimeSession", &std::make_shared<CRimeSession>)
      .function("initialize", &CRimeSession::Initialize)
      .function("hello", &CRimeSession::Hello)
      .function("processKey", &CRimeSession::ProcessKey)
      .function("getContext", &CRimeSession::GetContext)
      .function("getStatus", &CRimeSession::GetStatus)
      .function("getCommit", &CRimeSession::GetCommit)
      ;
}