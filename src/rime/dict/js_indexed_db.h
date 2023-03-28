//
// Created by fydeos on 23-3-23.
//

#ifndef RIME_JS_INDEXED_DB_H
#define RIME_JS_INDEXED_DB_H

#include <rime/dict/db.h>

namespace rime {

  struct JsIndexedDbCursor;
  struct JsIndexedDbWrapper;


  class JsIndexedDb;

  class JsIndexedDbAccessor : public DbAccessor {
  public:
    JsIndexedDbAccessor();

    JsIndexedDbAccessor(an<JsIndexedDbWrapper> db,
                        const string &prefix);

    virtual ~JsIndexedDbAccessor();

    virtual bool Reset();

    virtual bool Jump(const string &key);

    virtual bool GetNextRecord(string *key, string *value);

    virtual bool exhausted();

  private:
    an<JsIndexedDbWrapper> db_;
    the<JsIndexedDbCursor> cursor_;
    bool is_metadata_query_ = false;
  };

  class JsIndexedDb : public Db,
                  public Transactional {
  public:
    JsIndexedDb(const string &file_name,
            const string &db_name,
            const string &db_type = "");

    virtual ~JsIndexedDb();

    virtual bool Remove();

    virtual bool Open();

    virtual bool OpenReadOnly();

    virtual bool Close();

    virtual bool Backup(const string &snapshot_file);

    virtual bool Restore(const string &snapshot_file);

    virtual bool CreateMetadata();

    virtual bool MetaFetch(const string &key, string *value);

    virtual bool MetaUpdate(const string &key, const string &value);

    virtual an<DbAccessor> QueryMetadata();

    virtual an<DbAccessor> QueryAll();

    virtual an<DbAccessor> Query(const string &key);

    virtual bool Fetch(const string &key, string *value);

    virtual bool Update(const string &key, const string &value);

    virtual bool Erase(const string &key);

    // Transactional
    virtual bool BeginTransaction();

    virtual bool AbortTransaction();

    virtual bool CommitTransaction();

  private:
    void Initialize();

    an<JsIndexedDbWrapper> db_;
    string db_type_;
  };

};

#endif //RIME_JS_INDEXED_DB_H
