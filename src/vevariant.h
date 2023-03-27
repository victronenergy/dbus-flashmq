#ifndef VEVARIANT_H
#define VEVARIANT_H

#include <dbus-1.0/dbus/dbus.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "vendor/json.hpp"

// Explicitely documented in velib_python
#define EMPTY_ARRAY_AS_NULL_VALUE_TYPE "i"
#define VALID_EMPTY_ARRAY_VALUE_TYPE "u"

enum class VeVariantType
{
    Unknown,
    IntegerSigned16,
    IntegerSigned32,
    IntegerSigned64,
    IntegerUnsigned8,
    IntegerUnsigned16,
    IntegerUnsigned32,
    IntegerUnsigned64,
    String,
    Double,
    Boolean,
    Array,
    Dict
};

class VeVariant;

// TODO: finish? Or remove?
class VeVariantArray
{
    std::unique_ptr<std::vector<VeVariant>> arr;
    char contained_dbus_type = 0;

public:
    VeVariantArray() = default;
    VeVariantArray(const VeVariantArray &other);
    VeVariantArray& operator=(const VeVariantArray &other);
};

/**
 * @brief The VeVariant class is a little less uniony and prone to type confusion than a normal variant, and is recursive.
 */
class VeVariant
{
    // TODO: put these unique pointers in separate class, so I can remove the custom assignment operator and such.
    //VeVariantArray arr2;
    std::unique_ptr<std::vector<VeVariant>> arr;
    std::unique_ptr<std::unordered_map<VeVariant, VeVariant>> dict;

    std::string str;

    // Not using DBusBasicValues because a C union is not default constructed, has no operators, etc.
    uint8_t       u8 = 0;
    dbus_int16_t  i16 = 0;
    dbus_uint16_t u16 = 0;
    dbus_int32_t  i32 = 0;
    dbus_uint32_t u32 = 0;
    dbus_bool_t   bool_val = 0; // must be 32 bit in dbus
    dbus_int64_t  i64 = 0;
    dbus_uint64_t u64 = 0;
    double d = 0.0;

    VeVariantType type = VeVariantType::Unknown;
    std::string contained_array_type_as_string;

    static std::unique_ptr<std::vector<VeVariant>> make_array(DBusMessageIter *iter);
    static std::unique_ptr<std::unordered_map<VeVariant, VeVariant>> make_dict(DBusMessageIter *iter);

public:
    VeVariant() = default;
    VeVariant(VeVariant &&other) = default;
    VeVariant(const VeVariant &other);
    VeVariant(DBusMessageIter *iter);
    VeVariant(const std::string &v);
    VeVariant(const char *s);
    VeVariant(const nlohmann::json &j);
    std::string as_text() const;
    nlohmann::json as_json_value() const;
    int as_int() const;
    int get_dbus_type() const;
    std::string get_dbus_type_as_string() const;
    std::string get_contained_type_as_string() const;
    void append_args_to_dbus_message(DBusMessageIter *iter) const;

    VeVariant& operator=(const VeVariant &other);
    bool operator==(const VeVariant &other) const;
    VeVariantType get_type() const;
    std::size_t hash() const;
    VeVariant &operator[](const VeVariant &v);
    VeVariant &get_dict_val(const VeVariant &key);
};



namespace std {

    template <>
    struct hash<VeVariant>
    {
        std::size_t operator()(const VeVariant& k) const
        {
            return k.hash();
        }
    };

}

#endif // VEVARIANT_H
