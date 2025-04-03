#include "exceptions.h"

using namespace dbus_flashmq;

ItemNotFound::ItemNotFound(const std::string &msg, const std::string &service, const std::string &dbus_like_path) :
     std::runtime_error(msg),
     service(service),
     dbus_like_path(dbus_like_path)
{

}
