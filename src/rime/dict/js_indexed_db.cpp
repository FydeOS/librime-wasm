//
// Created by fydeos on 23-3-23.
//

#ifdef __EMSCRIPTEN__
#include "js_indexed_db.h"
#include <rime/dict/user_db.h>
#include <emscripten.h>
#include <emscripten/val.h>
#include <rime/common.h>
#include <map>

using emscripten::EM_VAL;
using emscripten::val;

namespace rime {

  static const char* kMetaCharacter = "\x01";

  EM_ASYNC_JS(EM_VAL, js_idb_open, (const char* db_name), {
    try {
      db_name = "rime_leveldb_" + UTF8ToString(db_name);
      db = await Module.idb.openDB(db_name, 1, {
        upgrade(db, oldVersion, newVersion, transaction, event) {
          db.createObjectStore('values', {keyPath: "key"});
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
      db.close();
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
        return allocateUTF8(result.value);
      } else {
        return 0;
      }
    } catch (e) {
      return 0;
    }
  })

  EM_ASYNC_JS(EM_VAL, js_idb_get_all, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      const result = await db.getAll('values');
      return Emval.toHandle(result);
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
        await db.put('values', { key: key, value: value });
      } else {
        await db.delete('values', key);
      }
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_ASYNC_JS(bool, js_idb_commit_transaction, (EM_VAL db_handle, EM_VAL tx_handle), {
    try {
      const db = Emval.toValue(db_handle);
      const txContent = Emval.toValue(tx_handle);
      const tx = db.transaction('values', 'readwrite');
      for (let op of txContent) {
        if (op.type == 'put') {
          tx.store.put({value: op.value, key: op.key});
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

  /*
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
   */


  struct JsIndexedDbWrapper : std::enable_shared_from_this<JsIndexedDbWrapper> {
    emscripten::val db_handle;
    std::map<string, string> data;
    struct operation {
      bool put;
      string key;
      string value;
    };
    std::vector<operation> current_transaction;
    bool readonly_;
  private:
    EM_VAL db() {
      return db_handle.as_handle();
    }
  public:

    bool Open(const string& file_name, bool readonly) {
      auto v = js_idb_open(file_name.c_str());
      db_handle = val::take_ownership(v);
      readonly_ = readonly;
      if (!db_handle.isNull()) {
        data.clear();
        // read all data to map
        auto all_val = val::take_ownership(js_idb_get_all(db()));
        int len = all_val["length"].as<int>();
        for (int i = 0; i < len; i++) {
          val obj = all_val[i];
          data[obj["key"].as<string>()] = obj["value"].as<string>();
        }
        return true;
      }
      return false;
    }

    void Release() {
      js_idb_close(db());
      data.clear();
      current_transaction.clear();
    }

    bool Fetch(const string& key, string* value) {
      auto find_result = data.find(key);
      if (find_result != data.end()) {
        *value = find_result->second;
        return true;
      } else {
        return false;
      }
    }

    bool Update(const string& key, const string& value, bool write_batch) {
      if (write_batch) {
        current_transaction.push_back({true, key, value});
        return true;
      } else {
        data[key] = value;
        return js_idb_put(db(), key.c_str(), value.c_str());
      }
    }

    bool Erase(const string& key, bool write_batch) {
      if (write_batch) {
        current_transaction.push_back({false, key});
        return true;
      } else {
        data.erase(key);
        return js_idb_put(db(), key.c_str(), nullptr);
      }
    }

    void CreateTransaction() {
      current_transaction.clear();
    }

    void AbortTransaction() {
      current_transaction.clear();
    }

    bool CommitTransaction() {
      if (current_transaction.empty())
        return true;
      val tx_info = val::array();
      for (int i = 0; i < current_transaction.size(); i++) {
        val cur_tx = val::object();
        string& key = current_transaction[i].key;
        string& val = current_transaction[i].value;
        if (current_transaction[i].put) {
          cur_tx.set("type", "put");
          data[key] = val;
        } else {
          cur_tx.set("type", "del");
          data.erase(key);
        }
        cur_tx.set("key", key);
        cur_tx.set("value", val);
        tx_info.set(i, cur_tx);
      }
      return js_idb_commit_transaction(db(), tx_info.as_handle());
    }
  };

  struct JsIndexedDbCursor {

  private:
    an<JsIndexedDbWrapper> db_;
    std::map<string, string>::const_iterator cursor;
    std::string last_pos;

  public:
    JsIndexedDbCursor(an<JsIndexedDbWrapper> db) : db_(db) {
    }

    bool IsValid() const {
      return cursor != db_->data.cend();
    }

    string GetKey() const {
      return cursor->first;
    }

    string GetValue() const {
      return cursor->second;
    }

    void Next() {
      cursor = std::next(cursor);
    }

    void Jump(const string& key) {
      cursor = db_->data.lower_bound(key);
    }

    void Reset() {
      cursor = db_->data.cbegin();
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
  }

  bool JsIndexedDbAccessor::Reset() {
    if (prefix_ != "") {
      cursor_->Jump(prefix_);
    } else {
      Reset();
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