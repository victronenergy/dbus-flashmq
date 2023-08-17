#include "vevariant.h"

#include <sstream>
#include <cassert>

#include "vendor/flashmq_plugin.h"
#include "exceptions.h"
#include "dbusmessageiteropencontainerguard.h"
#include "dbusmessageitersignature.h"

VeVariantArray::VeVariantArray(const VeVariantArray &other)
{
    *this = other;
}

VeVariantArray &VeVariantArray::operator=(const VeVariantArray &other)
{
    if (other.arr)
    {
        this->arr = std::make_unique<std::vector<VeVariant>>();
        (*this->arr) = (*other.arr);
    }

    this->contained_dbus_type = other.contained_dbus_type;

    return *this;
}

VeVariant::VeVariant(DBusMessageIter *iter)
{
    int dbus_type = dbus_message_iter_get_arg_type(iter);

    DBusMessageIter variant_iter;
    DBusMessageIter *_iter = iter;

    if (dbus_type == DBUS_TYPE_VARIANT)
    {
        dbus_message_iter_recurse(iter, &variant_iter);
        _iter = &variant_iter;
        dbus_type = dbus_message_iter_get_arg_type(&variant_iter);
    }

    DBusBasicValue value;

    // All the calls to dbus_message_iter_get_basic() looks like code duplication, but it's the safest way to avoid
    // a crash on trying to read a value from a non-basic type. In other words: there is no function to ask whether
    // the type is basic.
    switch (dbus_type)
    {
    case DBUS_TYPE_INT16:
        dbus_message_iter_get_basic(_iter, &value);
        this->i16 = value.i16;
        this->type = VeVariantType::IntegerSigned16;
        break;
    case DBUS_TYPE_UINT16:
        dbus_message_iter_get_basic(_iter, &value);
        this->u16 = value.u16;
        this->type = VeVariantType::IntegerUnsigned16;
        break;
    case DBUS_TYPE_INT32:
        dbus_message_iter_get_basic(_iter, &value);
        this->i32 = value.i32;
        this->type = VeVariantType::IntegerSigned32;
        break;
    case DBUS_TYPE_UINT32:
        dbus_message_iter_get_basic(_iter, &value);
        this->u32 = value.u32;
        this->type = VeVariantType::IntegerUnsigned32;
        break;
    case DBUS_TYPE_BOOLEAN:
        dbus_message_iter_get_basic(_iter, &value);
        this->bool_val = value.bool_val;
        this->type = VeVariantType::Boolean;
        break;
    case DBUS_TYPE_INT64:
        dbus_message_iter_get_basic(_iter, &value);
        this->i64 = value.i64;
        this->type = VeVariantType::IntegerSigned64;
        break;
    case DBUS_TYPE_UINT64:
        dbus_message_iter_get_basic(_iter, &value);
        this->u64 = value.u64;
        this->type = VeVariantType::IntegerUnsigned64;
        break;
    case DBUS_TYPE_DOUBLE:
        dbus_message_iter_get_basic(_iter, &value);
        this->d = value.dbl;
        this->type = VeVariantType::Double;
        break;
    case DBUS_TYPE_BYTE:
        dbus_message_iter_get_basic(_iter, &value);
        this->u8 = value.byt;
        this->type = VeVariantType::IntegerUnsigned8;
        break;
    case DBUS_TYPE_STRING:
        dbus_message_iter_get_basic(_iter, &value);
        this->str = value.str;
        this->type = VeVariantType::String;
        break;
    case DBUS_TYPE_STRUCT:
        flashmq_logf(LOG_WARNING, "Struct not implemented. In C++, it would have to be a map/array to be dynamic");
        break;
    case DBUS_TYPE_ARRAY:
    {
        // You can't tell an array and dict apart from the outside, so we have to peek in.
        DBusMessageIter peek_iter;
        dbus_message_iter_recurse(_iter, &peek_iter);
        const int array_type = dbus_message_iter_get_arg_type(&peek_iter);

        DBusMessageIterSignature signature(&peek_iter);
        this->contained_array_type_as_string = signature.signature;

        if (array_type == DBUS_TYPE_DICT_ENTRY)
        {
            this->type = VeVariantType::Dict;
            this->dict = make_dict(_iter);
        }
        else
        {
            this->type = VeVariantType::Array;
            arr = make_array(_iter);
        }

        break;
    }
    case DBUS_TYPE_DICT_ENTRY:
        flashmq_logf(LOG_WARNING, "DBUS_TYPE_DICT_ENTRY in variant is not supported..");
        break;
    case DBUS_TYPE_VARIANT:
        flashmq_logf(LOG_WARNING, "Variant in variant is not supported.");
        break;
    default:
        break;
    }
}

VeVariant::VeVariant(const std::string &v) :
    str(v),
    type(VeVariantType::String)
{

}

VeVariant::VeVariant(const char *s) :
    str(s),
    type(VeVariantType::String)
{

}

/**
 * @brief VeVariant::VeVariant
 * @param j
 *
 * It uses 32 bit ints if it fits, otherwise 64. Just like velib_python.
 */
VeVariant::VeVariant(const nlohmann::json &j)
{
    if (j.is_number_integer())
    {
        const dbus_int64_t v = j.get<dbus_int64_t>();
        if (v & 0x7FFFFFFF00000000)
        {
            this->i64 = v;
            this->type = VeVariantType::IntegerSigned64;
        }
        else
        {
            const dbus_int32_t v = j.get<dbus_int32_t>();
            this->i32 = v;
            this->type = VeVariantType::IntegerSigned32;
        }
    }
    else if (j.is_string())
    {
        this->str = j.get<std::string>();
        this->type = VeVariantType::String;
    }
    else if (j.is_number_float())
    {
        this->d = j.get<double>();;
        this->type = VeVariantType::Double;
    }
    else if (j.is_boolean())
    {
        this->bool_val = j.get<bool>();
        this->type = VeVariantType::Boolean;
    }
    else if (j.is_array())
    {
        // We can't know here is someone is really trying to write an empty array, or null. We also support json null now; see below.

        this->arr = std::make_unique<std::vector<VeVariant>>();
        type = VeVariantType::Array;
        contained_array_type_as_string = VALID_EMPTY_ARRAY_VALUE_TYPE;

        bool type_anchored = false;
        int last_type = 0;

        for (const nlohmann::json &v : j)
        {
            VeVariant v2(v);

            if (type_anchored && last_type != v2.get_dbus_type())
                throw ValueError("Dbus doesn't support arrays of mixed type");

            this->arr->push_back(std::move(v2));

            contained_array_type_as_string = v2.get_dbus_type_as_string();

            type_anchored = true;
            last_type = v2.get_dbus_type();
        }
    }
    else if (j.is_object())
    {

    }
    else if (j.is_null())
    {
        this->arr = std::make_unique<std::vector<VeVariant>>();
        type = VeVariantType::Array;
        contained_array_type_as_string = EMPTY_ARRAY_AS_NULL_VALUE_TYPE;
    }
    else
    {
        const std::string type_name(j.type_name());
        throw ValueError("Unsupported JSON value type: " + type_name);
    }
}

VeVariant::VeVariant(const VeVariant &other)
{
    *this = other;
}

std::unique_ptr<std::vector<VeVariant> > VeVariant::make_array(DBusMessageIter *iter)
{
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
        throw ValueError("Calling make_array() on something other than array.");

    const int n = dbus_message_iter_get_element_count(iter);
    std::unique_ptr<std::vector<VeVariant>> result = std::make_unique<std::vector<VeVariant>>();
    result->reserve(n);

    DBusMessageIter array_iter;
    dbus_message_iter_recurse(iter, &array_iter);
    iter = nullptr;

    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID)
    {
        result->emplace_back(&array_iter);
        dbus_message_iter_next(&array_iter);
    }

    return result;
}

std::unique_ptr<std::unordered_map<VeVariant, VeVariant>> VeVariant::make_dict(DBusMessageIter *iter)
{
    if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
        throw ValueError("Calling make_dict() on something other than array.");

    std::unique_ptr<std::unordered_map<VeVariant, VeVariant>> result = std::make_unique<std::unordered_map<VeVariant, VeVariant>>();
    std::unordered_map<VeVariant, VeVariant> &r = *result;

    DBusMessageIter array_iter;
    dbus_message_iter_recurse(iter, &array_iter);
    iter = nullptr;

    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID)
    {
        if (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_DICT_ENTRY)
            throw ValueError("Make_dict() needs dict entries in array.");

        DBusMessageIter dict_iter;
        dbus_message_iter_recurse(&array_iter, &dict_iter);

        VeVariant key(&dict_iter);
        dbus_message_iter_next(&dict_iter);
        VeVariant val(&dict_iter);

        r[key] = val;

        dbus_message_iter_next(&array_iter);
    }

    return result;
}

std::string VeVariant::as_text() const
{
    std::ostringstream o;

    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
        o << i16;
        break;
    case VeVariantType::IntegerSigned32:
        o << i32;
        break;
    case VeVariantType::IntegerSigned64:
        o << i64;
        break;
    case VeVariantType::IntegerUnsigned8:
        o << u8;
        break;
    case VeVariantType::IntegerUnsigned16:
        o << u16;
        break;
    case VeVariantType::IntegerUnsigned32:
        o << u32;
        break;
    case VeVariantType::IntegerUnsigned64:
        o << u64;
        break;
    case VeVariantType::String:
    {
        o << str;
        break;
    }
    case VeVariantType::Double:
        o << d;
        break;
    case VeVariantType::Boolean:
        o << std::boolalpha << bool_val;
        break;
    case VeVariantType::Array:
    case VeVariantType::Dict:
    {
        auto v = as_json_value();
        o << v.dump();
        break;
    }
    default:
        return "unknown type";
    }

    std::string result(o.str());
    return result;
}

nlohmann::json VeVariant::as_json_value() const
{
    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
        return i16;
    case VeVariantType::IntegerSigned32:
        return i32;
    case VeVariantType::IntegerSigned64:
        return i64;
    case VeVariantType::IntegerUnsigned8:
        return u8;
    case VeVariantType::IntegerUnsigned16:
        return u16;
    case VeVariantType::IntegerUnsigned32:
        return u32;
    case VeVariantType::IntegerUnsigned64:
        return u64;
    case VeVariantType::String:
        return str;
    case VeVariantType::Double:
        return d;
    case VeVariantType::Boolean:
    {
        // TODO: I don't actually know if the the current dbus-mqtt uses 1/0 or true/false.
        bool v = static_cast<bool>(bool_val);
        return v;
    }
    case VeVariantType::Array:
    {
        // The array being null when we think we are an array shouldn't happen, but playing it safe.
        if (!this->arr)
            return nlohmann::json();

        nlohmann::json array = nlohmann::json::array({});

        if (this->arr)
        {
            // I disabled the EMPTY_ARRAY_AS_NULL_VALUE_TYPE check, because there are dbus services that don't seem to stick to that rule.
            if (this->arr->empty()) // && this->contained_array_type_as_string == EMPTY_ARRAY_AS_NULL_VALUE_TYPE)
                return nlohmann::json();

            for (VeVariant &v : *this->arr)
            {
                array.push_back(v.as_json_value());
            }
        }

        return array;
    }
    case VeVariantType::Dict:
    {
        nlohmann::json obj = nlohmann::json::object();

        if (this->dict)
        {
            for (auto &p : *this->dict)
            {
                const VeVariant &key = p.first;

                if (key.get_type() != VeVariantType::String)
                    throw ValueError("JSON dict keys must be string.");

                obj[key.as_json_value()] = p.second.as_json_value();
            }
        }

        return obj;
    }
    default:
        return "unknown_type";
    }
}

// Kind of a placeholder. It may need to be templated if I use it more seriously.
int VeVariant::as_int() const
{
    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
        return i16;
    case VeVariantType::IntegerSigned32:
        return i32;
    case VeVariantType::IntegerSigned64:
        return i64;
    case VeVariantType::IntegerUnsigned8:
        return u8;
    case VeVariantType::IntegerUnsigned16:
        return u16;
    case VeVariantType::IntegerUnsigned32:
        return u32;
    case VeVariantType::IntegerUnsigned64:
        return u64;
    case VeVariantType::Double:
        return static_cast<int>(d);
    case VeVariantType::Boolean:
        return static_cast<int>(bool_val);
        break;
    default:
        return 0;
    }

    return 0;
}

int VeVariant::get_dbus_type() const
{
    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
        return DBUS_TYPE_INT16;
    case VeVariantType::IntegerSigned32:
        return DBUS_TYPE_INT32;
    case VeVariantType::IntegerSigned64:
        return DBUS_TYPE_INT64;
    case VeVariantType::IntegerUnsigned8:
        return DBUS_TYPE_BYTE;
    case VeVariantType::IntegerUnsigned16:
        return DBUS_TYPE_UINT16;
    case VeVariantType::IntegerUnsigned32:
        return DBUS_TYPE_UINT32;
    case VeVariantType::IntegerUnsigned64:
        return DBUS_TYPE_UINT64;
    case VeVariantType::String:
        return DBUS_TYPE_STRING;
    case VeVariantType::Double:
        return DBUS_TYPE_DOUBLE;
    case VeVariantType::Boolean:
        return DBUS_TYPE_BOOLEAN;
    case VeVariantType::Array:
        return DBUS_TYPE_ARRAY;
    case VeVariantType::Dict:
        flashmq_logf(LOG_WARNING, "Using get_dbus_type() on a dict. This is not a dbus type, so what are you trying to do?");
        return DBUS_TYPE_ARRAY; // TODO: array with dict types. Hmm. How?
    default:
        return DBUS_TYPE_INVALID;
    }
}

std::string VeVariant::get_dbus_type_as_string() const
{
    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
        return DBUS_TYPE_INT16_AS_STRING;
    case VeVariantType::IntegerSigned32:
        return DBUS_TYPE_INT32_AS_STRING;
    case VeVariantType::IntegerSigned64:
        return DBUS_TYPE_INT64_AS_STRING;
    case VeVariantType::IntegerUnsigned8:
        return DBUS_TYPE_BYTE_AS_STRING;
    case VeVariantType::IntegerUnsigned16:
        return DBUS_TYPE_UINT16_AS_STRING;
    case VeVariantType::IntegerUnsigned32:
        return DBUS_TYPE_UINT32_AS_STRING;
    case VeVariantType::IntegerUnsigned64:
        return DBUS_TYPE_UINT64_AS_STRING;
    case VeVariantType::String:
        return DBUS_TYPE_STRING_AS_STRING;
    case VeVariantType::Double:
        return DBUS_TYPE_DOUBLE_AS_STRING;
    case VeVariantType::Boolean:
        return DBUS_TYPE_BOOLEAN_AS_STRING;
    case VeVariantType::Array:
        return DBUS_TYPE_ARRAY_AS_STRING;
    case VeVariantType::Dict:
        return DBUS_TYPE_ARRAY_AS_STRING; // TODO: array with dict types. Hmm. How?
    default:
        return DBUS_TYPE_INVALID_AS_STRING;
    }
}

/**
 * @brief VeVariant::get_contained_type_as_string is only needed for arrays.
 * @return
 */
std::string VeVariant::get_contained_type_as_string() const
{
    std::string recursive = this->contained_array_type_as_string;

    if (arr && !arr->empty())
    {
        const VeVariant &first = arr->front();
        recursive += first.get_contained_type_as_string();
    }

    return recursive;
}

/**
 * @brief VeVariant::append_args_to_dbus_message is to be used for appending the VeVariant as argument to dbus messages, like method calls.
 * @param iter
 */
void VeVariant::append_args_to_dbus_message(DBusMessageIter *iter) const
{
    switch (this->type)
    {
    case VeVariantType::IntegerSigned16:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &i16);
        break;
    }
    case VeVariantType::IntegerSigned32:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &i32);
        break;
    }
    case VeVariantType::IntegerSigned64:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &i64);
        break;
    }
    case VeVariantType::IntegerUnsigned8:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &u8);
        break;
    }
    case VeVariantType::IntegerUnsigned16:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &u16);
        break;
    }
    case VeVariantType::IntegerUnsigned32:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &u32);
        break;
    }
    case VeVariantType::IntegerUnsigned64:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &u64);
        break;
    }
    case VeVariantType::String:
    {
        const char *mystr = str.c_str();
        dbus_message_iter_append_basic(iter, get_dbus_type(), &mystr);
        break;
    }
    case VeVariantType::Double:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &d);
        break;
    }
    case VeVariantType::Boolean:
    {
        dbus_message_iter_append_basic(iter, get_dbus_type(), &bool_val);
        break;
    }
    case VeVariantType::Array:
    {
        if (!arr) // Empty pointer shouldn't happen, but just in case.
        {
            DBusMessageIterOpenContainerGuard array_iter(iter, DBUS_TYPE_ARRAY, VALID_EMPTY_ARRAY_VALUE_TYPE);
        }
        else
        {
            DBusMessageIterOpenContainerGuard array_iter(iter, DBUS_TYPE_ARRAY, get_contained_type_as_string().c_str());
            int last_type = 0;
            bool type_anchored = false;
            for(const VeVariant &v : *arr)
            {
                if (type_anchored && v.get_dbus_type() != last_type)
                    throw ValueError("You can't put different types in a dbus array.");

                v.append_args_to_dbus_message(array_iter.get_array_iter());
                last_type = v.get_dbus_type();
                type_anchored = true;
            }
        }

        break;
    }
    case VeVariantType::Dict:
        // TODO: do we need to be able to set dicts?
        throw ValueError("append_args() for dict is not supported. Yet?");
    default:
        throw ValueError("Default case in append_args(). You shouldn't get here.");
    }
}

VeVariant &VeVariant::operator=(const VeVariant &other)
{
    if (other.arr)
    {
        this->arr = std::make_unique<std::vector<VeVariant>>();
        (*this->arr) = (*other.arr);
    }
    else
    {
        this->arr.reset();
    }

    if (other.dict)
    {
        this->dict = std::make_unique<std::unordered_map<VeVariant, VeVariant>>();
        (*this->dict) = (*other.dict);
    }
    else
    {
        this->dict.reset();
    }

    this->u8 = other.u8;
    this->i16 = other.i16;
    this->u16 = other.u16;
    this->i32 = other.i32;
    this->u32 = other.u32;
    this->bool_val = other.bool_val;
    this->i64 = other.i64;
    this->u64 = other.u64;
    this->str = other.str;
    this->d = other.d;
    this->type = other.type;
    this->contained_array_type_as_string = other.contained_array_type_as_string;

    return *this;
}

bool VeVariant::operator==(const VeVariant &other) const
{
    if (this->type != other.type)
        return false;

    switch (type)
    {
    case VeVariantType::Unknown:
        return true;
    case VeVariantType::IntegerSigned16:
        return this->i16 == other.i16;
    case VeVariantType::IntegerSigned32:
        return this->i32 == other.i32;
    case VeVariantType::IntegerSigned64:
        return this->i64 == other.i64;
    case VeVariantType::IntegerUnsigned8:
        return this->u8 == other.u8;
    case VeVariantType::IntegerUnsigned16:
        return this->u16 == other.u16;
    case VeVariantType::IntegerUnsigned32:
        return this->u32 == other.u32;
    case VeVariantType::IntegerUnsigned64:
        return this->u64 == other.u64;
    case VeVariantType::String:
        return this->str == other.str;
    case VeVariantType::Double:
        return this->d == other.d;
    case VeVariantType::Boolean:
        return this->bool_val == other.bool_val;
    case VeVariantType::Array:
    {
        if (!this->arr && !other.arr)
            return true;

        if (!this->arr || !other.arr)
            return false;

        return *this->arr == *other.arr;
    }
    case VeVariantType::Dict:
    {
        if (!this->dict && !other.dict)
            return true;

        if (!this->dict || !other.dict)
            return false;

        return *this->dict == *other.dict;
    }
    default:
        throw ValueError("Shouldn't end up here.");
    }
}

VeVariantType VeVariant::get_type() const
{
    return type;
}

std::size_t VeVariant::hash() const
{
    switch (type)
    {
    case VeVariantType::Unknown:
        return std::hash<int>()(0);
    case VeVariantType::IntegerSigned16:
        return std::hash<dbus_int16_t>()(0);
    case VeVariantType::IntegerSigned32:
        return std::hash<dbus_int32_t>()(0);
    case VeVariantType::IntegerSigned64:
        return std::hash<dbus_int64_t>()(0);
    case VeVariantType::IntegerUnsigned8:
        return std::hash<uint8_t>()(0);
    case VeVariantType::IntegerUnsigned16:
        return std::hash<dbus_uint16_t>()(0);
    case VeVariantType::IntegerUnsigned32:
        return std::hash<dbus_uint32_t>()(0);
    case VeVariantType::IntegerUnsigned64:
        return std::hash<dbus_uint64_t>()(0);
    case VeVariantType::String:
        return std::hash<std::string>()(str);
    case VeVariantType::Double:
        return std::hash<double>()(d);
    case VeVariantType::Boolean:
        return std::hash<dbus_bool_t>()(bool_val);
    case VeVariantType::Array:
    {
        if (!this->arr)
            return std::hash<int>()(0);

        size_t hash = 0;

        for (const VeVariant &v : *this->arr)
        {
            hash ^= v.hash();
        }

        return hash;
    }
    default:
        throw ValueError("Can't calculcate hash more complicated than array.");
    }
}

VeVariant &VeVariant::operator[](const VeVariant &v)
{
    if (type != VeVariantType::Dict || !dict)
        throw ValueError("VeVariant is not a dict or the dict is null.");
    std::unordered_map<VeVariant, VeVariant> &d = *dict;
    return d[v];
}

VeVariant &VeVariant::get_dict_val(const VeVariant &key)
{
    if (type != VeVariantType::Dict || !dict)
        throw ValueError("VeVariant is not a dict or the dict is null.");

    auto pos = dict->find(key);
    if (pos == dict->end())
        throw ValueError("Key '" + key.as_text() + "' not found.");

    return pos->second;
}


