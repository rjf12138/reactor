#ifndef __JSON_TYPE_H__
#define __JSON_TYPE_H__

#include "basic_head.h"
#include "byte_buffer.h"
#include "logger.h"

namespace basic {
// json中重要的分割字符
extern const std::vector<char> sperate_chars;

class JsonValue;

enum ValueType {
    JSON_UNKNOWN_TYPE = -1000,
    JSON_NUMBER_TYPE = 10001,
    JSON_BOOL_TYPE   = 10002,
    JSON_NULL_TYPE   = 10003,
    JSON_STRING_TYPE = 10004,
    JSON_ARRAY_TYPE  = 10005,
    JSON_OBJECT_TYPE = 10006
};

class JsonType {
public:
    JsonType(void) {}
    virtual ~JsonType(void) {}

    static ValueType check_value_type(ByteBuffer::iterator &iter);
    static std::string value_type_to_string(ValueType type);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) {
        return ByteBuffer::iterator();
    }
    virtual std::string to_string(void) const { return ""; }
};

// ======================== JsonNumber ========================
class JsonNumber : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonNumber &rhs);
public:
    explicit JsonNumber(void);
    explicit JsonNumber(const double &val);
    ~JsonNumber(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    double   to_double(void) const { return value_; }
    uint64_t to_uint(void)   const { return static_cast<uint64_t>(value_); }
    int64_t  to_int(void)    const { return static_cast<int64_t>(value_); }

    bool operator==(const JsonNumber& rhs) const;
    bool operator!=(const JsonNumber& rhs) const;
    bool operator==(const double& rhs) const;
    bool operator!=(const double& rhs) const;
    JsonNumber& operator=(JsonNumber rhs);

private:
    double value_;
};

// ======================== JsonBool ========================
class JsonBool : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonBool &rhs);
public:
    explicit JsonBool(void);
    explicit JsonBool(bool val);
    ~JsonBool(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    bool to_bool(void) const { return value_; }

    bool operator==(const JsonBool& rhs) const;
    bool operator!=(const JsonBool& rhs) const;
    bool operator==(const bool& rhs) const;
    bool operator!=(const bool& rhs) const;
    JsonBool& operator=(JsonBool rhs);

private:
    bool value_;
};

// ======================== JsonNull ========================
class JsonNull : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonNull &rhs);
public:
    explicit JsonNull(void);
    ~JsonNull(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    bool operator==(const JsonNull& rhs) const;
    bool operator!=(const JsonNull& rhs) const;
    JsonNull& operator=(JsonNull rhs);

private:
    std::string value_;
};

// ======================== JsonString ========================
class JsonString : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonString &rhs);
public:
    explicit JsonString(void);
    explicit JsonString(const std::string &val);
    explicit JsonString(const char *val);
    ~JsonString(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    // 返回字符串的原始值(未转义),用于键名存储、类型转换等内部用途
    const std::string& raw_value(void) const { return value_; }

    bool operator==(const JsonString& rhs) const;
    bool operator!=(const JsonString& rhs) const;
    bool operator==(const std::string& rhs) const;
    bool operator!=(const std::string& rhs) const;
    JsonString& operator=(JsonString rhs);

private:
    std::string value_;
};

// ======================== JsonValue (前向声明需要) ========================
class JsonValue;

// ======================== JsonObject ========================
class JsonObject : public JsonType {
public:
    friend class JsonValue;
    friend std::ostream& operator<<(std::ostream &os, JsonObject &rhs);
    typedef std::map<std::string, JsonValue>::iterator       iterator;
    typedef std::map<std::string, JsonValue>::const_iterator const_iterator;
public:
    JsonObject(void);
    JsonObject(const JsonObject &jobj);
    ~JsonObject(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    // 查找
    iterator       find(const std::string &key);
    const_iterator find(const std::string &key) const;
    bool           contains(const std::string &key) const;

    // 删除
    int        erase(const std::string &key);
    iterator   erase(iterator &remove_iter);

    // 添加
    int add(const std::string &key, const JsonValue &value);

    // 容量
    int  size(void)  const { return static_cast<int>(value_.size()); }
    bool empty(void) const { return value_.empty(); }
    void clear(void)       { value_.clear(); }

    // 操作符
    bool        operator==(const JsonObject& rhs) const;
    bool        operator!=(const JsonObject& rhs) const;
    JsonObject& operator=(const JsonObject &rhs);
    JsonValue&  operator[](const std::string &key);
    const JsonValue& operator[](const std::string &key) const;

    // 迭代器
    iterator       begin();
    const_iterator begin() const;
    iterator       end();
    const_iterator end()   const;

private:
    std::map<std::string, JsonValue> value_;
};

// ======================== JsonArray ========================
class JsonArray : public JsonType {
public:
    friend class JsonValue;
    friend std::ostream& operator<<(std::ostream &os, JsonArray &rhs);
    typedef std::vector<JsonValue>::iterator       iterator;
    typedef std::vector<JsonValue>::const_iterator const_iterator;
public:
    JsonArray(void);
    JsonArray(const JsonArray &jarr);
    ~JsonArray(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const override;

    // 删除
    iterator erase(const int &index);
    iterator erase(const iterator &remove_iter);

    // 添加
    int add(const JsonValue &value);

    // 容量
    int  size(void)  const { return static_cast<int>(value_.size()); }
    bool empty(void) const { return value_.empty(); }
    void clear(void)       { value_.clear(); }

    // 操作符
    JsonValue&       operator[](size_t key);
    const JsonValue& operator[](size_t key) const;
    bool             operator==(const JsonArray& rhs) const;
    bool             operator!=(const JsonArray& rhs) const;
    JsonArray&       operator=(const JsonArray &rhs);

    // 迭代器
    iterator       begin();
    const_iterator begin() const;
    iterator       end();
    const_iterator end()   const;

private:
    std::vector<JsonValue> value_;
};

// ======================== JsonValue ========================
class JsonValue {
    friend class JsonObject;
    friend class JsonArray;
public:
    JsonValue(void);
    JsonValue(const JsonBool &value);
    JsonValue(const JsonNumber &value);
    JsonValue(const JsonString &value);
    JsonValue(const JsonObject &value);
    JsonValue(const JsonArray &value);
    JsonValue(const JsonNull &value);
    JsonValue(const JsonValue &value);

    JsonValue(const bool &value);
    JsonValue(const int &value);
    JsonValue(const double &value);
    JsonValue(const std::string &value);
    JsonValue(const char *value);

    ~JsonValue(void);

    // ---- 类型转换操作符（深拷贝） ----
    operator JsonNumber();
    operator JsonString();
    operator JsonBool();
    operator JsonObject();
    operator JsonArray();
    operator JsonNull();

    // ---- 赋值 ----
    JsonValue& operator=(const JsonBool &val);
    JsonValue& operator=(const JsonNumber &val);
    JsonValue& operator=(const JsonString &val);
    JsonValue& operator=(const JsonObject &val);
    JsonValue& operator=(const JsonArray &val);
    JsonValue& operator=(const JsonNull &val);
    JsonValue& operator=(const JsonValue &val);

    bool operator==(const JsonValue& rhs) const;
    bool operator!=(const JsonValue& rhs) const;
    bool operator==(const std::string &rhs) const;
    bool operator!=(const std::string &rhs) const;
    bool operator==(const char *rhs) const;
    bool operator!=(const char *rhs) const;

    JsonValue&       operator[](const std::string &key);
    const JsonValue& operator[](const std::string &key) const;
    JsonValue&       operator[](const int &key);
    const JsonValue& operator[](const int &key) const;

    // ---- 类型查询 ----
    ValueType type(void) const { return type_; }
    bool is_null(void)   const { return type_ == JSON_NULL_TYPE; }
    bool is_number(void) const { return type_ == JSON_NUMBER_TYPE; }
    bool is_bool(void)   const { return type_ == JSON_BOOL_TYPE; }
    bool is_string(void) const { return type_ == JSON_STRING_TYPE; }
    bool is_array(void)  const { return type_ == JSON_ARRAY_TYPE; }
    bool is_object(void) const { return type_ == JSON_OBJECT_TYPE; }

    // ---- 序列化 ----
    std::string to_string(void) const;

    // ---- 取值（类型不匹配时抛异常） ----
    int64_t  to_int(void)    const;
    double   to_double(void) const;
    JsonArray&  to_array(void);
    const JsonArray& to_array(void) const;
    JsonObject&  to_object(void);
    const JsonObject& to_object(void) const;

    // ---- 容量（通过内部对象/数组类型代理） ----
    int  size(void)  const;
    bool empty(void) const;

    // ---- 复制 ----
    void copy(const JsonValue &val, bool is_release = true);

    // ---- 内部访问（供 friend 类使用） ----
    ValueType internal_type(void) const { return type_; }
    JsonType* internal_ptr(void)  const { return value_; }

protected:
    ValueType type_;
    JsonType *value_;
};

// ======================== WeJson ========================
class WeJson : public JsonValue
{
public:
    WeJson(void);
    WeJson(const std::string &json);
    WeJson(const ByteBuffer &data);
    virtual ~WeJson(void);

    virtual int parse(const ByteBuffer &data);
    virtual int parse(const std::string &data);

    virtual std::string to_string(void);
    virtual std::string format_json(void);

    void create_object(void);
    void create_array(void);

    ValueType get_type(void) const { return JsonValue::type(); }

    JsonObject& get_object(void);
    JsonArray&  get_array(void);

private:
    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos);
};

}

#endif