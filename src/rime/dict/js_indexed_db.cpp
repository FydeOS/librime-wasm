//
// Created by fydeos on 23-3-23.
//

#ifdef __EMSCRIPTEN__
#include "js_indexed_db.h"
#include <rime/dict/user_db.h>
#include <emscripten.h>
#include <emscripten/val.h>
#include <rime/common.h>

using emscripten::EM_VAL;
using emscripten::val;

namespace rime {

  static const char* kMetaCharacter = "\x01";

  EM_ASYNC_JS(EM_VAL, js_idb_open, (const char* db_name), {
    try {
      db_name = "rime_leveldb_" + UTF8ToString(db_name);
      db = await Module.idb.openDB(db_name, 1, {
        upgrade(db, oldVersion, newVersion, transaction, event) {
          db.createObjectStore('values');
        },
        terminated() {
        },
      });

      return Emval.toHandle(db);
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_idb_delete_db, (const char* db_name), {
    try {
      db_name = "rime_leveldb_" + UTF8ToString(db_name);
      await Module.idb.deleteDB(db_name, { });
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })


  EM_JS(void, js_idb_close, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      Module.idb.unwrap(db).close();
    } catch (e) {
      out(e);
    }
  })

  EM_ASYNC_JS(char*, js_idb_get, (EM_VAL db_handle, const char* key), {
    try {
      const db = Emval.toValue(db_handle);
      key = UTF8ToString(key);
      const result = await db.get('values', key);
      if (result != undefined) {
        return allocateUTF8(result);
      } else {
        return 0;
      }
    } catch (e) {
      return 0;
    }
  })

  EM_ASYNC_JS(bool, js_idb_put, (EM_VAL db_handle, const char* key, const char* value), {
    try {
      const db = Emval.toValue(db_handle);
      key = UTF8ToString(key);
      if (value != 0) {
        value = UTF8ToString(value);
        await db.put('values', value, key);
      } else {
        await db.delete('values', key);
      }
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_ASYNC_JS(bool, js_idb_tx_put, (EM_VAL tx_handle, const char* key, const char* value), {
    try {
      const tx = Emval.toValue(tx_handle);
      key = UTF8ToString(key);
      if (value != 0) {
        value = UTF8ToString(value);
        tx.op.push({type: 'put', key: key, value: value});
      } else {
        tx.op.push({type: 'del', key: key});
      }
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_JS(EM_VAL, js_idb_create_transaction, (EM_VAL db_handle, bool readonly), {
    try {
      const db = Emval.toValue(db_handle);
      return Emval.toHandle({db: db, op: []});
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_idb_commit_transaction, (EM_VAL tx_handle), {
    try {
      const txContent = Emval.toValue(tx_handle);
      const tx = txContent.db.transaction('values', 'readwrite');
      for (let op of txContent.op) {
        if (op.type == 'put') {
          tx.store.put(op.value, op.key);
        } else if (op.type == 'del') {
          tx.store.del(op.key);
        }
      }
      await tx.done;

      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_JS(void, js_idb_abort_transaction, (EM_VAL tx_handle), {
    try {
    } catch (e) {
      out(e);
    }
  })

  EM_ASYNC_JS(EM_VAL, js_idb_create_cursor, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      const cursor = await db.transaction('values').store.openCursor();
      return Emval.toHandle(cursor);
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_idb_cursor_continue, (EM_VAL cursor_handle, const char* pos), {
    try {
      let cursor = Emval.toValue(cursor_handle);
      if (pos) {
        const to_pos = UTF8ToString(pos);
        await cursor.continue(to_pos);
      } else {
        await cursor.continue();
      }
      return true;
    } catch (e) {
      if (!e.message.includes("has iterated past its end"))
        out(e);
      return false;
    }
  })

  EM_JS(char *, js_idb_cursor_get_val, (EM_VAL cursor_handle, bool value), {
    try {
      const cursor = Emval.toValue(cursor_handle);
      const result = value ? cursor.value : cursor.key;
      return allocateUTF8(result);
    } catch (e) {
      out(e);
      return null;
    }
  })


  struct JsIndexedDbWrapper : std::enable_shared_from_this<JsIndexedDbWrapper> {
    emscripten::val db_handle;
    emscripten::val tx_handle;
    bool readonly_;
  private:
    EM_VAL db() {
      return db_handle.as_handle();
    }
    EM_VAL tx() {
      return tx_handle.as_handle();
    }

  public:

    bool Open(const string& file_name, bool readonly) {
      auto v = js_idb_open(file_name.c_str());
      db_handle = val::take_ownership(v);
      readonly_ = readonly;
      return db_handle.as_handle() != nullptr;
    }

    void Release() {
      js_idb_close(db());
    }

    val CreateCursor() {
      return val::take_ownership(js_idb_create_cursor(db()));
    }

    bool Fetch(const string& key, string* value) {
      char* p = js_idb_get(db(), key.c_str());
      if (p) {
        *value = string(p);
        free(p);
        return true;
      } else {
        return false;
      }
    }

    bool Update(const string& key, const string& value, bool write_batch) {
      if (write_batch) {
        return js_idb_tx_put(tx(), key.c_str(), value.c_str());
      } else {
        return js_idb_put(db(), key.c_str(), value.c_str());
      }
    }

    bool Erase(const string& key, bool write_batch) {
      if (write_batch) {
        return js_idb_tx_put(tx(), key.c_str(), nullptr);
      } else {
        return js_idb_put(db(), key.c_str(), nullptr);
      }
    }

    void CreateTransaction() {
      auto v = js_idb_create_transaction(db(), readonly_);
      tx_handle = val::take_ownership(v);
    }

    void AbortTransaction() {
      tx_handle = val::null();
    }

    bool CommitTransaction() {
      return js_idb_commit_transaction(tx_handle.as_handle());
    }
  };

  struct JsIndexedDbCursor {

  private:
    an<JsIndexedDbWrapper> db_;
    val cursor;
    std::string last_pos;
    EM_VAL handle() const {
      return cursor.as_handle();
    }

  public:
    JsIndexedDbCursor(an<JsIndexedDbWrapper> db) : db_(db) {
      Release();
    }

    bool IsValid() const {
      return !cursor.isNull();
    }

    string GetKey() const {
      char* buf = js_idb_cursor_get_val(handle(), false);
      string result(buf);
      free(buf);
      return result;
    }

    string GetValue() const {
      char* buf = js_idb_cursor_get_val(handle(), true);
      string result(buf);
      free(buf);
      return result;
    }

    void Next() {
      if (!IsValid()) {
        LOG(WARNING) << "Attempt to call next() when ptr is invalid";
        return;
      }
      bool ok = js_idb_cursor_continue(handle(), nullptr);
      if (!ok) {
        cursor = val::null();
      } else {
        last_pos = GetKey();
      }
    }

    void Jump(const string& key) {
      if (key.compare(last_pos) <= 0) {
        cursor = db_->CreateCursor();
      } else {
        if (cursor.isNull()) {
          // e.g. last item of database is "b", querying "bb" will make cursor null, then querying "bba" will reach here.
          // if querying "a" after querying "bb", cursor will be rebuilt above
          return;
        }
      }
      bool ok = js_idb_cursor_continue(handle(), key.c_str());
      if (!ok) {
        cursor = val::null();
      } else {
        last_pos = GetKey();
      }
    }

    void Release() {
      cursor = val::null();
      last_pos = "\xFF\xFF";
    }
  };

// JsIndexedDbAccessor memebers

  JsIndexedDbAccessor::JsIndexedDbAccessor() {
  }

  JsIndexedDbAccessor::JsIndexedDbAccessor(an<JsIndexedDbWrapper> db,
                                   const string& prefix)
      : DbAccessor(prefix), db_(db),
        is_metadata_query_(prefix == kMetaCharacter) {
    cursor_ = std::make_unique<JsIndexedDbCursor>(db);
  }

  JsIndexedDbAccessor::~JsIndexedDbAccessor() {
    cursor_->Release();
  }

  bool JsIndexedDbAccessor::Reset() {
    cursor_->Release();
    if (prefix_ != "") {
      cursor_->Jump(prefix_);
    }
    return cursor_->IsValid();
  }

  bool JsIndexedDbAccessor::Jump(const string& key) {
    cursor_->Jump(key);
    return cursor_->IsValid();
  }

  bool JsIndexedDbAccessor::GetNextRecord(string* key, string* value) {
    if (!cursor_->IsValid() || !key || !value)
      return false;
    *key = cursor_->GetKey();
    if (!MatchesPrefix(*key)) {
      return false;
    }
    if (is_metadata_query_) {
      key->erase(0, 1);  // remove meta character
    }
    *value = cursor_->GetValue();
    cursor_->Next();
    return true;
  }

  bool JsIndexedDbAccessor::exhausted() {
    return !cursor_->IsValid() || !MatchesPrefix(cursor_->GetKey());
  }

// JsIndexedDb members

  JsIndexedDb::JsIndexedDb(const string& file_name,
                   const string& db_name,
                   const string& db_type)
      : Db(file_name, db_name),
        db_type_(db_type) {
  }

  JsIndexedDb::~JsIndexedDb() {
    if (loaded())
      Close();
  }

  void JsIndexedDb::Initialize() {
    db_ = New<JsIndexedDbWrapper>();
  }

  an<DbAccessor> JsIndexedDb::QueryMetadata() {
    return Query(kMetaCharacter);
  }

  an<DbAccessor> JsIndexedDb::QueryAll() {
    an<DbAccessor> all = Query("");
    if (all)
      all->Jump(" ");  // skip metadata
    return all;
  }

  an<DbAccessor> JsIndexedDb::Query(const string& key) {
    if (!loaded())
      return nullptr;
    return New<JsIndexedDbAccessor>(db_, key);
  }

  bool JsIndexedDb::Fetch(const string& key, string* value) {
    if (!value || !loaded())
      return false;
    return db_->Fetch(key, value);
  }

  bool JsIndexedDb::Update(const string& key, const string& value) {
    if (!loaded() || readonly())
      return false;
    DLOG(INFO) << "update db entry: " << key << " => " << value;
    return db_->Update(key, value, in_transaction());
  }

  bool JsIndexedDb::Erase(const string& key) {
    if (!loaded() || readonly())
      return false;
    DLOG(INFO) << "erase db entry: " << key;
    return db_->Erase(key, in_transaction());
  }

  bool JsIndexedDb::Backup(const string& snapshot_file) {
    if (!loaded())
      return false;
    LOG(INFO) << "backing up db '" << name() << "' to " << snapshot_file;
    // TODO(chen): suppose we only use this method for user dbs.
    bool success = UserDbHelper(this).UniformBackup(snapshot_file);
    if (!success) {
      LOG(ERROR) << "failed to create snapshot file '" << snapshot_file
                 << "' for db '" << name() << "'.";
    }
    return success;
  }

  bool JsIndexedDb::Restore(const string& snapshot_file) {
    if (!loaded() || readonly())
      return false;
    // TODO(chen): suppose we only use this method for user dbs.
    bool success = UserDbHelper(this).UniformRestore(snapshot_file);
    if (!success) {
      LOG(ERROR) << "failed to restore db '" << name()
                 << "' from '" << snapshot_file << "'.";
    }
    return success;
  }

  bool JsIndexedDb::Remove() {
    if (loaded()) {
      LOG(ERROR) << "attempt to remove opened db '" << name() << "'.";
      return false;
    }
    auto status = js_idb_delete_db(file_name().c_str());
    if (!status) {
      LOG(ERROR) << "Error removing db '" << name();
      return false;
    }
    return true;
  }

  bool JsIndexedDb::Open() {
    if (loaded())
      return false;
    Initialize();
    readonly_ = false;
    auto status = db_->Open(file_name(), readonly_);
    loaded_ = status;

    if (loaded_) {
      string db_name;
      if (!MetaFetch("/db_name", &db_name)) {
        if (!CreateMetadata()) {
          LOG(ERROR) << "error creating metadata.";
          Close();
        }
      }
    }
    else {
      LOG(ERROR) << "Error opening db '" << name();
    }
    return loaded_;
  }

  bool JsIndexedDb::OpenReadOnly() {
    if (loaded())
      return false;
    Initialize();
    readonly_ = true;
    auto status = db_->Open(file_name(), readonly_);
    loaded_ = status;

    if (!loaded_) {
      LOG(ERROR) << "Error opening db '" << name() << "' read-only.";
    }
    return loaded_;
  }

  bool JsIndexedDb::Close() {
    if (!loaded())
      return false;

    db_->Release();

    LOG(INFO) << "closed db '" << name() << "'.";
    loaded_ = false;
    readonly_ = false;
    in_transaction_ = false;
    return true;
  }

  bool JsIndexedDb::CreateMetadata() {
    return Db::CreateMetadata() &&
           MetaUpdate("/db_type", db_type_);
  }

  bool JsIndexedDb::MetaFetch(const string& key, string* value) {
    return Fetch(kMetaCharacter + key, value);
  }

  bool JsIndexedDb::MetaUpdate(const string& key, const string& value) {
    return Update(kMetaCharacter + key, value);
  }

  bool JsIndexedDb::BeginTransaction() {
    if (!loaded())
      return false;
    db_->CreateTransaction();
    in_transaction_ = true;
    return true;
  }

  bool JsIndexedDb::AbortTransaction() {
    if (!loaded() || !in_transaction())
      return false;
    db_->AbortTransaction();
    in_transaction_ = false;
    return true;
  }

  bool JsIndexedDb::CommitTransaction() {
    if (!loaded() || !in_transaction())
      return false;
    bool ok = db_->CommitTransaction();
    in_transaction_ = false;
    return ok;
  }

  template <>
  string UserDbComponent<JsIndexedDb>::extension() const {
    return ".userdb";
  }

  template <>
  UserDbWrapper<JsIndexedDb>::UserDbWrapper(const string& file_name,
                                        const string& db_name)
      : JsIndexedDb(file_name, db_name, "userdb") {
  }

}  // namespace rime

#endif