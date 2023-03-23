//
// Created by fydeos on 23-3-23.
//

#ifdef __EMSCRIPTEN__
#include "js_leveldb.h"
#include <rime/dict/user_db.h>
#include <emscripten.h>
#include <emscripten/val.h>

using emscripten::EM_VAL;
using emscripten::val;

namespace rime {

  static const char* kMetaCharacter = "\x01";

  EM_JS(EM_VAL, js_ldb_open, (const char* db_name), {
    try {
      db_name = UTF8ToString(db_name);
      /*
      db = await Module..openDB(db_name, 1, {
        upgrade(db, oldVersion, newVersion, transaction, event) {
          db.createObjectStore('values');
        },
        terminated() {
        },
      });
       */
      db = new Module.BrowserLevel(db_name, {keyEncoding: 'utf8', valueEncoding: 'utf8'});

      return Emval.toHandle(db);
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_ldb_delete_db, (const char* db_name), {
    try {
      db_name = UTF8ToString(db_name);
      await Module.BrowserLevel.destroy(db_name);
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })


  EM_ASYNC_JS(void, js_ldb_close, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      await db.close();
    } catch (e) {
      out(e);
    }
  })

  EM_ASYNC_JS(char*, js_ldb_get, (EM_VAL db_handle, const char* key_handle), {
    try {
      const db = Emval.toValue(db_handle);
      const key = UTF8ToString(key_handle);
      const result = await db.get(key);
      if (result != undefined) {
        return allocateUTF8(result);
      } else {
        return 0;
      }
    } catch (e) {
      if (!e.message.includes("Entry not found"))
        out(e);
      return 0;
    }
  })

  EM_ASYNC_JS(bool, js_ldb_put, (EM_VAL db_handle, const char* key, const char* value), {
    try {
      const db = Emval.toValue(db_handle);
      key = UTF8ToString(key);
      if (value != 0) {
        value = UTF8ToString(value);
        await db.put(key, value);
      } else {
        await db.del(key);
      }
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_ASYNC_JS(bool, js_ldb_tx_put, (EM_VAL tx_handle, const char* key, const char* value), {
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

  EM_JS(EM_VAL, js_ldb_create_transaction, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      return Emval.toHandle({db: db, op: []});
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_ldb_commit_transaction, (EM_VAL tx_handle), {
    try {
      const tx = Emval.toValue(tx_handle);
      await tx.db.batch(tx.op);
      return true;
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_JS(void, js_ldb_abort_transaction, (EM_VAL tx_handle), {
  })

  EM_ASYNC_JS(EM_VAL, js_ldb_create_iterator, (EM_VAL db_handle), {
    try {
      const db = Emval.toValue(db_handle);
      const iterator = await db.iterator();
      return Emval.toHandle({it: iterator, value: null, key: null});
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_JS(void, js_ldb_iterator_seek, (EM_VAL iterator_handle, const char* pos), {
    try {
      let iterator = Emval.toValue(iterator_handle);
      iterator.it.seek(UTF8ToString(pos));
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_ASYNC_JS(bool, js_ldb_iterator_next, (EM_VAL iterator_handle), {
    try {
      let iterator = Emval.toValue(iterator_handle);
      const data = await iterator.it.next();
      if (data != undefined) {
        iterator.key = data[0];
        iterator.value = data[1];
        return true;
      } else {
        iterator.key = null;
        iterator.value = null;
        return false;
      }
    } catch (e) {
      out(e);
      return false;
    }
  })

  EM_JS(char *, js_ldb_iterator_get_val, (EM_VAL iterator_handle, bool value), {
    try {
      const iterator = Emval.toValue(iterator_handle);
      const result = value ? iterator.value : iterator.key;
      return allocateUTF8(result);
    } catch (e) {
      out(e);
      return null;
    }
  })

  EM_JS(void, js_ldb_iterator_close, (EM_VAL iterator_handle), {
    try {
      const iterator = Emval.toValue(iterator_handle);
      iterator.it.close();
    } catch (e) {
      out(e);
    }
  })

  struct JsLevelDbIterator {

  private:
    val iterator;
    bool valid;
    EM_VAL handle() const {
      return iterator.as_handle();
    }

  public:
    JsLevelDbIterator(val iterator): iterator(iterator), valid(false) {
    }

    bool IsValid() const {
      return valid;
    }

    string GetKey() const {
      char* buf = js_ldb_iterator_get_val(handle(), false);
      string result(buf);
      free(buf);
      return result;
    }

    string GetValue() const {
      char* buf = js_ldb_iterator_get_val(handle(), true);
      string result(buf);
      free(buf);
      return result;
    }

    void Next() {
      valid = js_ldb_iterator_next(handle());
    }

    bool Jump(const string& key) {
      js_ldb_iterator_seek(handle(), key.c_str());
      Next();
      return valid;
    }

    void Release() {
      js_ldb_iterator_close(handle());
      iterator = val::null();
    }
  };

  struct JsLevelDbWrapper {
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
      auto v = js_ldb_open(file_name.c_str());
      db_handle = val::take_ownership(v);
      readonly_ = readonly;
      return db_handle.as_handle() != nullptr;
    }

    void Release() {
      js_ldb_close(db());
    }

    JsLevelDbIterator* CreateIterator() {
      auto iterator = val::take_ownership(js_ldb_create_iterator(db()));
      return new JsLevelDbIterator(iterator);
    }

    bool Fetch(const string& key, string* value) {
      char* p = js_ldb_get(db(), key.c_str());
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
        return js_ldb_tx_put(tx(), key.c_str(), value.c_str());
      } else {
        return js_ldb_put(db(), key.c_str(), value.c_str());
      }
    }

    bool Erase(const string& key, bool write_batch) {
      if (write_batch) {
        return js_ldb_tx_put(tx(), key.c_str(), nullptr);
      } else {
        return js_ldb_put(db(), key.c_str(), nullptr);
      }
    }

    void CreateTransaction() {
      auto v = js_ldb_create_transaction(db());
      tx_handle = val::take_ownership(v);
    }

    void AbortTransaction() {
      js_ldb_abort_transaction(tx_handle.as_handle());
    }

    bool CommitTransaction() {
      return js_ldb_commit_transaction(tx_handle.as_handle());
    }
  };

// JsLevelDbAccessor memebers

  JsLevelDbAccessor::JsLevelDbAccessor() {
  }

  JsLevelDbAccessor::JsLevelDbAccessor(JsLevelDbIterator* iterator,
                                   const string& prefix)
      : DbAccessor(prefix), iterator_(iterator),
        is_metadata_query_(prefix == kMetaCharacter) {
    Reset();
  }

  JsLevelDbAccessor::~JsLevelDbAccessor() {
    iterator_->Release();
  }

  bool JsLevelDbAccessor::Reset() {
    return iterator_->Jump(prefix_);
  }

  bool JsLevelDbAccessor::Jump(const string& key) {
    return iterator_->Jump(key);
  }

  bool JsLevelDbAccessor::GetNextRecord(string* key, string* value) {
    if (!iterator_->IsValid() || !key || !value)
      return false;
    *key = iterator_->GetKey();
    if (!MatchesPrefix(*key)) {
      return false;
    }
    if (is_metadata_query_) {
      key->erase(0, 1);  // remove meta character
    }
    *value = iterator_->GetValue();
    iterator_->Next();
    return true;
  }

  bool JsLevelDbAccessor::exhausted() {
    return !iterator_->IsValid() || !MatchesPrefix(iterator_->GetKey());
  }

// JsLevelDb members

  JsLevelDb::JsLevelDb(const string& file_name,
                   const string& db_name,
                   const string& db_type)
      : Db(file_name, db_name),
        db_type_(db_type) {
  }

  JsLevelDb::~JsLevelDb() {
    if (loaded())
      Close();
  }

  void JsLevelDb::Initialize() {
    db_.reset(new JsLevelDbWrapper);
  }

  an<DbAccessor> JsLevelDb::QueryMetadata() {
    return Query(kMetaCharacter);
  }

  an<DbAccessor> JsLevelDb::QueryAll() {
    an<DbAccessor> all = Query("");
    if (all)
      all->Jump(" ");  // skip metadata
    return all;
  }

  an<DbAccessor> JsLevelDb::Query(const string& key) {
    if (!loaded())
      return nullptr;
    return New<JsLevelDbAccessor>(db_->CreateIterator(), key);
  }

  bool JsLevelDb::Fetch(const string& key, string* value) {
    if (!value || !loaded())
      return false;
    return db_->Fetch(key, value);
  }

  bool JsLevelDb::Update(const string& key, const string& value) {
    if (!loaded() || readonly())
      return false;
    DLOG(INFO) << "update db entry: " << key << " => " << value;
    return db_->Update(key, value, in_transaction());
  }

  bool JsLevelDb::Erase(const string& key) {
    if (!loaded() || readonly())
      return false;
    DLOG(INFO) << "erase db entry: " << key;
    return db_->Erase(key, in_transaction());
  }

  bool JsLevelDb::Backup(const string& snapshot_file) {
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

  bool JsLevelDb::Restore(const string& snapshot_file) {
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

  bool JsLevelDb::Remove() {
    if (loaded()) {
      LOG(ERROR) << "attempt to remove opened db '" << name() << "'.";
      return false;
    }
    auto status = js_ldb_delete_db(file_name().c_str());
    if (!status) {
      LOG(ERROR) << "Error removing db '" << name();
      return false;
    }
    return true;
  }

  bool JsLevelDb::Open() {
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

  bool JsLevelDb::OpenReadOnly() {
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

  bool JsLevelDb::Close() {
    if (!loaded())
      return false;

    db_->Release();

    LOG(INFO) << "closed db '" << name() << "'.";
    loaded_ = false;
    readonly_ = false;
    in_transaction_ = false;
    return true;
  }

  bool JsLevelDb::CreateMetadata() {
    return Db::CreateMetadata() &&
           MetaUpdate("/db_type", db_type_);
  }

  bool JsLevelDb::MetaFetch(const string& key, string* value) {
    return Fetch(kMetaCharacter + key, value);
  }

  bool JsLevelDb::MetaUpdate(const string& key, const string& value) {
    return Update(kMetaCharacter + key, value);
  }

  bool JsLevelDb::BeginTransaction() {
    if (!loaded())
      return false;
    db_->CreateTransaction();
    in_transaction_ = true;
    return true;
  }

  bool JsLevelDb::AbortTransaction() {
    if (!loaded() || !in_transaction())
      return false;
    db_->AbortTransaction();
    in_transaction_ = false;
    return true;
  }

  bool JsLevelDb::CommitTransaction() {
    if (!loaded() || !in_transaction())
      return false;
    bool ok = db_->CommitTransaction();
    in_transaction_ = false;
    return ok;
  }

  template <>
  string UserDbComponent<JsLevelDb>::extension() const {
    return ".userdb";
  }

  template <>
  UserDbWrapper<JsLevelDb>::UserDbWrapper(const string& file_name,
                                        const string& db_name)
      : JsLevelDb(file_name, db_name, "userdb") {
  }

}  // namespace rime

#endif