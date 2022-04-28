#ifndef __JSON_TYPE_H__
#define __JSON_TYPE_H__

#include "basic_head.h"
#include "byte_buffer.h"
#include "logger.h"

namespace basic {
// json中重要的分割字符
const std::vector<char> sperate_chars = {' ', '\r', '\n','\t','{', '}','[', ']',',',':','"'};

class JsonValue;

enum ValueType {
    JSON_UNKNOWN_TYPE = -1000,
    JSON_NUMBER_TYPE = 10001,
    JSON_BOOL_TYPE = 10002,
    JSON_NULL_TYPE = 10003,
    JSON_STRING_TYPE = 10004,
    JSON_ARRAY_TYPE = 10005,
    JSON_OBJECT_TYPE = 10006
};

class JsonType {
public:
    JsonType(void) {}
    virtual ~JsonType(void) {}

    // 检查当前位置的字符来判断接下来的是什么类型，具体参考doc中的资料
    static ValueType check_value_type(ByteBuffer::iterator &iter);

    // ValueType 转为字符串表示
    static std::string value_type_to_string(ValueType type);

    // 解析遇到的类型，具体取决于check_value_type()返回的类型和继承该类的实现
    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos){return ByteBuffer::iterator();}
    
    // 将json值反序列化为字符串输出， 没有格式化
    virtual std::string to_string(void) { return "";}
};

// json 数值类型
class JsonNumber : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonNumber &rhs);
public:
    explicit JsonNumber(void);
    explicit JsonNumber(const double &val);
    ~JsonNumber(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) override;

    double to_double(void) const  {return value_;}
    uint64_t to_uint(void) const {return static_cast<uint64_t>(value_);}
    int64_t to_int(void) const {return static_cast<int64_t>(value_);}

    bool operator==(const JsonNumber& rhs) const;
    bool operator!=(const JsonNumber& rhs) const;
    bool operator==(const double& rhs) const;
    bool operator!=(const double& rhs) const;
    JsonNumber& operator=(JsonNumber rhs);

private:
    double        value_;
};

// json 布尔类型
class JsonBool : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonBool &rhs);
public:
    explicit JsonBool(void);
    explicit JsonBool(bool val);
    ~JsonBool(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const;

    bool to_bool(void) const  {return value_;}

    bool operator==(const JsonBool& rhs) const;
    bool operator!=(const JsonBool& rhs) const;
    bool operator==(const bool& rhs) const;
    bool operator!=(const bool& rhs) const;
    JsonBool& operator=(JsonBool rhs);

private:
    bool value_;
};

// json null 类型
class JsonNull : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonNull &rhs);
public:
    explicit JsonNull(void);
    ~JsonNull(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const;

    bool operator==(const JsonNull& rhs) const;
    bool operator!=(const JsonNull& rhs) const;
    JsonNull& operator=(JsonNull rhs);

private:
    std::string value_;
};

// json 字符串类型
class JsonString : public JsonType {
    friend std::ostream& operator<<(std::ostream &os, JsonString &rhs);
public:
    explicit JsonString(void);
    explicit JsonString(std::string val);
    explicit JsonString(const char *val);
    ~JsonString(void);

    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) const;

    bool operator==(const JsonString& rhs) const;
    bool operator!=(const JsonString& rhs) const;
    bool operator==(const std::string& rhs) const;
    bool operator!=(const std::string& rhs) const;
    JsonString& operator=(JsonString rhs);

private:
    std::string value_;
};

class JsonValue;
class JsonObject : public JsonType {
public:
    friend class JsonValue;
    friend std::ostream& operator<<(std::ostream &os, JsonObject &rhs);
    typedef std::map<std::string, JsonValue>::iterator iterator;
public:
    explicit JsonObject(void);
    ~JsonObject(void);

    // 序列化和反序列化
    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) override;
    
    // 查找元素
    JsonObject::iterator find(const std::string &key);
    // 删除元素
    int erase(const std::string &key);
    JsonObject::iterator erase(JsonObject::iterator &remove_iter);
    // 当前类型为对象时添加元素
    int add(const std::string &key, const JsonValue &value);
    // 返回元素数量
    int size(void) const {return value_.size();}
    // 清空元素
    void clear(void) {value_.clear();}

    // 重载操作符
    bool operator==(const JsonObject& rhs) const;
    bool operator!=(const JsonObject& rhs) const;
    JsonObject& operator=(const JsonObject &rhs);
    JsonValue& operator[](const std::string &key);

    // 迭代器
    iterator begin();
    iterator end();

private:
    std::map<std::string, JsonValue> value_;
};

// json 数组类型
class JsonArray : public JsonType {
public:
    friend class JsonValue;
    friend std::ostream& operator<<(std::ostream &os, JsonArray &rhs);
    typedef std::vector<JsonValue>::iterator iterator;
public:
    explicit JsonArray(void);
    ~JsonArray(void);

    // 序列化和反序列化
    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos) override;
    virtual std::string to_string(void) override;

    // 数组或是对象删除元素
    iterator erase(const int &index);
    iterator erase(const iterator &remove_iter);
    // 当前添加元素
    int add(const JsonValue &value);
    // 返回元素数量
    int size(void) const {return value_.size();}
    // 清空元素
    void clear(void) {value_.clear();}

    // 重载操作符
    JsonValue& operator[](size_t key);
    const JsonValue& operator[](const size_t key) const;
    bool operator==(const JsonArray& rhs) const;
    bool operator!=(const JsonArray& rhs) const; 
    JsonArray& operator=(const JsonArray &rhs);

    // 迭代器
    iterator begin();
    iterator end();
private:
    std::vector<JsonValue> value_;
};

// json中转类型：可以安装当前存储的类型输出或是接收不同的类型
class JsonValue {
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

    operator JsonNumber();
    operator JsonString();
    operator JsonBool();
    operator JsonObject();
    operator JsonArray();
    operator JsonNull();

    JsonValue& operator=(const JsonBool &val);
    JsonValue& operator=(const JsonNumber &val);
    JsonValue& operator=(const JsonString &val);
    JsonValue& operator=(const JsonObject &val);
    JsonValue& operator=(const JsonArray &val);
    JsonValue& operator=(const JsonNull &val);
    JsonValue& operator=(const JsonValue &val);

    bool operator==(const JsonValue& rhs) const;
    bool operator!=(const JsonValue& rhs) const;

    JsonValue& operator[](const std::string &key);
    JsonValue& operator[](const int &key);

    ValueType type(void) const {return type_;}
    std::string to_string(void);

public:
    void copy(const JsonValue &val, bool is_release = true);

public:
    ValueType type_;
    JsonType *value_;
};

class WeJson : public JsonValue 
{
public:
    WeJson(void);
    WeJson(const std::string &json);
    WeJson(const ByteBuffer &data);
    virtual ~WeJson(void);

    // 解析保存在ByteBuffer的数据
    virtual int parse(const ByteBuffer &data);
    // 解析保存在string中的数据
    virtual int parse(const std::string &data);

    // 非格式化输出 json
    virtual std::string to_string(void);
    // 格式化输出 json
    virtual std::string format_json(void);

    // 构建成object
    void create_object(void);
    // 构建成array
    void create_array(void);

    // 返回当前对象类型
    ValueType get_type(void) const {return JsonValue::type();}

    // 构建成object
    JsonObject& get_object(void);
    // 构建成array
    JsonArray& get_array(void);

private:
    virtual ByteBuffer::iterator parse(ByteBuffer::iterator &value_start_pos, ByteBuffer::iterator &json_end_pos);
};

}

#endif