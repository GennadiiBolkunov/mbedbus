#include "mbedbus/Skeleton.h"
#include "mbedbus/Log.h"

namespace mbedbus {

    Skeleton::Skeleton(std::shared_ptr<Connection> conn, IntrospectedNode node)
        : conn_(std::move(conn))
        , node_(std::move(node))
    {}

    std::shared_ptr<Skeleton> Skeleton::fromXml(std::shared_ptr<Connection> conn,
        const std::string& xml) {
        auto node = IntrospectionParser::parse(xml);
        if (node.name.empty())
            throw Error("com.mbedbus.Error", "XML has no node name (object path)");
        return std::shared_ptr<Skeleton>(new Skeleton(std::move(conn), std::move(node)));
    }

    std::shared_ptr<Skeleton> Skeleton::fromNode(std::shared_ptr<Connection> conn,
        const IntrospectedNode& node) {
        if (node.name.empty())
            throw Error("com.mbedbus.Error", "Node has no name (object path)");
        return std::shared_ptr<Skeleton>(new Skeleton(std::move(conn), node));
    }

    void Skeleton::setMethodHandler(const std::string& iface, const std::string& method,
        std::function<Message(const Message&)> handler) {
        methodBindings_[iface][method].handler = std::move(handler);
    }

    void Skeleton::setPropertyGetter(const std::string& iface, const std::string& prop,
        std::function<Variant()> getter) {
        propertyBindings_[iface][prop].getter = std::move(getter);
    }

    void Skeleton::setPropertySetter(const std::string& iface, const std::string& prop,
        std::function<void(const Variant&)> setter) {
        propertyBindings_[iface][prop].setter = std::move(setter);
    }

    void Skeleton::bindRawMethod(const std::string& iface, const std::string& method,
        std::function<Message(const Message&)> handler) {
        // No signature check for raw binding
        auto* m = node_.findMethod(iface, method);
        if (!m) throw Error("com.mbedbus.Error",
                "Method " + iface + "." + method + " not found in XML");
        setMethodHandler(iface, method, std::move(handler));
    }

    void Skeleton::finalize(SkeletonMode mode) {
        if (obj_) throw Error("com.mbedbus.Error", "Skeleton already finalized");

        obj_ = ServiceObject::create(conn_, node_.name);

        for (auto& iface : node_.interfaces) {
            // Skip standard D-Bus interfaces — ServiceObject handles them automatically
            if (iface.name == "org.freedesktop.DBus.Introspectable" ||
                iface.name == "org.freedesktop.DBus.Peer" ||
                iface.name == "org.freedesktop.DBus.Properties" ||
                iface.name == "org.freedesktop.DBus.ObjectManager")
                continue;


            // Register methods
            for (auto& method : iface.methods) {
                auto bindIt = methodBindings_.find(iface.name);
                std::function<Message(const Message&)> handler;

                if (bindIt != methodBindings_.end()) {
                    auto mIt = bindIt->second.find(method.name);
                    if (mIt != bindIt->second.end())
                        handler = mIt->second.handler;
                }

                if (!handler) {
                    if (mode == SkeletonMode::Strict) {
                        throw Error("com.mbedbus.Error",
                            "No handler bound for method " + iface.name + "." + method.name);
                    }
                    // Lenient: stub that returns NotImplemented
                    std::string fullName = iface.name + "." + method.name;
                    handler = [fullName](const Message& call) -> Message {
                        return Message::createError(call,
                            "org.freedesktop.DBus.Error.NotSupported",
                            "Method " + fullName + " is not implemented");
                    };
                }

                // We need to add the method via the low-level InterfaceBuilder path.
                // Since InterfaceBuilder::addMethod expects a lambda with typed args,
                // and we have a raw handler, we use a workaround: register a lambda
                // that receives no args and internally delegates to the raw handler.
                // But this won't work because the message dispatcher matches by name.
                // Instead, we add the method data directly.
                // The handler is already in the right form: Message(const Message&).
                MethodInfo mi;
                mi.name = method.name;
                mi.inputSig = method.inputSignature();
                mi.outputSig = method.outputSignature();
                mi.handler = handler;
                for (auto& a : method.inArgs)
                    mi.inArgs.emplace_back(std::make_pair(a.name, a.signature));
                for (auto& a : method.outArgs)
                    mi.outArgs.emplace_back(std::make_pair(a.name, a.signature));
                // Access internal interface data directly
                auto& ifaceData = obj_->mutableInterfaces()[iface.name];
                obj_->mutableInterfaces()[iface.name];
                ifaceData.name = iface.name;
                ifaceData.methods.push_back(std::move(mi));
            }

            // Register properties
            for (auto& prop : iface.properties) {
                PropertyInfo pi;
                pi.name = prop.name;
                pi.signature = prop.signature;
                pi.readable = (prop.access == PropertyAccess::Read ||
                               prop.access == PropertyAccess::ReadWrite);
                pi.writable = (prop.access == PropertyAccess::Write ||
                               prop.access == PropertyAccess::ReadWrite);

                auto bindIt = propertyBindings_.find(iface.name);
                if (bindIt != propertyBindings_.end()) {
                    auto pIt = bindIt->second.find(prop.name);
                    if (pIt != bindIt->second.end()) {
                        pi.getter = pIt->second.getter;
                        pi.setter = pIt->second.setter;
                    }
                }

                if (pi.readable && !pi.getter) {
                    if (mode == SkeletonMode::Strict) {
                        throw Error("com.mbedbus.Error",
                            "No getter bound for readable property " +
                            iface.name + "." + prop.name);
                    }
                    std::string fullName = iface.name + "." + prop.name;
                    pi.getter = [fullName]() -> Variant {
                        throw Error("org.freedesktop.DBus.Error.NotSupported",
                            "Property " + fullName + " getter not implemented");
                    };
                }

                if (pi.writable && !pi.setter) {
                    if (mode == SkeletonMode::Strict) {
                        throw Error("com.mbedbus.Error",
                            "No setter bound for writable property " +
                            iface.name + "." + prop.name);
                    }
                    std::string fullName = iface.name + "." + prop.name;
                    pi.setter = [fullName](const Variant&) {
                        throw Error("org.freedesktop.DBus.Error.NotSupported",
                            "Property " + fullName + " setter not implemented");
                    };
                }

                auto& ifaceData = obj_->mutableInterfaces()[iface.name];
                obj_->mutableInterfaces()[iface.name];
                ifaceData.name = iface.name;
                ifaceData.properties.push_back(std::move(pi));
            }

            // Register signal declarations
            for (auto& sig : iface.signals) {
                SignalInfo si;
                si.name = sig.name;
                si.signature = sig.signature();
                for (auto& a : sig.args)
                    si.args.emplace_back(std::make_pair(a.name, a.signature));
                auto& ifaceData = obj_->mutableInterfaces()[iface.name];
                obj_->mutableInterfaces()[iface.name];
                ifaceData.signals.push_back(std::move(si));
            }
        }

        obj_->finalize();
        MBEDBUS_LOG("Skeleton finalized: %s", node_.name.c_str());
    }

} // namespace mbedbus