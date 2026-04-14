#include "mbedbus/IntrospectionParser.h"
#include "mbedbus/Error.h"

namespace mbedbus {

    std::string IntrospectionParser::extractAttr(const std::string& tag,
        const std::string& attrName) {
        for (char quote : {'"', '\''}) {
            std::string needle = attrName + "=" + quote;
            auto pos = tag.find(needle);
            if (pos != std::string::npos) {
                pos += needle.size();
                auto end = tag.find(quote, pos);
                if (end != std::string::npos)
                    return tag.substr(pos, end - pos);
            }
        }
        return "";
    }

    std::string IntrospectionParser::extractTagName(const std::string& tag) {
        std::string::size_type start = (tag[0] == '/') ? 1 : 0;
        std::string::size_type end = start;
        while (end < tag.size() && tag[end] != ' ' && tag[end] != '>'
               && tag[end] != '/' && tag[end] != '\t' && tag[end] != '\n')
            ++end;
        return tag.substr(start, end - start);
    }

    IntrospectedNode IntrospectionParser::parse(const std::string& xml) {
        IntrospectedNode node;

        // Parsing state: which container are we inside?
        IntrospectedInterface* curIface = nullptr;
        IntrospectedMethod* curMethod = nullptr;
        IntrospectedSignal* curSignal = nullptr;
        IntrospectedProperty* curProperty = nullptr;
        IntrospectedArg* curArg = nullptr;

        std::string::size_type pos = 0;
        while (pos < xml.size()) {
            auto tagStart = xml.find('<', pos);
            if (tagStart == std::string::npos) break;

            // Skip comments, PIs, DOCTYPE
            if (xml.compare(tagStart, 4, "<!--") == 0) {
                pos = xml.find("-->", tagStart + 4);
                pos = (pos != std::string::npos) ? pos + 3 : xml.size();
                continue;
            }
            if (xml.compare(tagStart, 2, "<?") == 0) {
                pos = xml.find("?>", tagStart + 2);
                pos = (pos != std::string::npos) ? pos + 2 : xml.size();
                continue;
            }
            if (xml.compare(tagStart, 2, "<!") == 0) {
                pos = xml.find('>', tagStart + 2);
                pos = (pos != std::string::npos) ? pos + 1 : xml.size();
                continue;
            }

            auto tagEnd = xml.find('>', tagStart);
            if (tagEnd == std::string::npos) break;

            std::string tagContent = xml.substr(tagStart + 1, tagEnd - tagStart - 1);
            bool isClosing = !tagContent.empty() && tagContent[0] == '/';
            bool isSelfClosing = !tagContent.empty() && tagContent.back() == '/';

            std::string tagName = extractTagName(tagContent);

            if (isClosing) {
                if (tagName == "method")    curMethod = nullptr;
                else if (tagName == "signal")    curSignal = nullptr;
                else if (tagName == "interface") curIface = nullptr;
                else if (tagName == "property")  curProperty = nullptr;
                else if (tagName == "arg")       curArg = nullptr;
            } else {
                if (tagName == "node") {
                    std::string name = extractAttr(tagContent, "name");
                    if (node.name.empty() && !name.empty())
                        node.name = name;
                    else if (!name.empty())
                        node.childNodes.emplace_back(name);
                } else if (tagName == "interface") {
                    node.interfaces.emplace_back();
                    curIface = &node.interfaces.back();
                    curIface->name = extractAttr(tagContent, "name");
                    if (isSelfClosing) curIface = nullptr;
                } else if (tagName == "method" && curIface) {
                    curIface->methods.emplace_back();
                    curMethod = &curIface->methods.back();
                    curMethod->name = extractAttr(tagContent, "name");
                    if (isSelfClosing) curMethod = nullptr;
                } else if (tagName == "signal" && curIface) {
                    curIface->signals.emplace_back();
                    curSignal = &curIface->signals.back();
                    curSignal->name = extractAttr(tagContent, "name");
                    if (isSelfClosing) curSignal = nullptr;
                } else if (tagName == "property" && curIface) {
                    curIface->properties.emplace_back();
                    curProperty = &curIface->properties.back();
                    curProperty->name = extractAttr(tagContent, "name");
                    curProperty->signature = extractAttr(tagContent, "type");
                    std::string acc = extractAttr(tagContent, "access");
                    if (acc == "readwrite")     curProperty->access = PropertyAccess::ReadWrite;
                    else if (acc == "write")    curProperty->access = PropertyAccess::Write;
                    else                        curProperty->access = PropertyAccess::Read;
                    if (isSelfClosing) curProperty = nullptr;
                } else if (tagName == "arg") {
                    IntrospectedArg arg;
                    arg.name = extractAttr(tagContent, "name");
                    arg.signature = extractAttr(tagContent, "type");
                    std::string direction = extractAttr(tagContent, "direction");

                    IntrospectedArg* placed = nullptr;
                    if (curMethod) {
                        if (direction == "out")
                            curMethod->outArgs.emplace_back(std::move(arg));
                        else
                            curMethod->inArgs.emplace_back(std::move(arg));
                        placed = (direction == "out") ? &curMethod->outArgs.back()
                                                      : &curMethod->inArgs.back();
                    } else if (curSignal) {
                        curSignal->args.emplace_back(std::move(arg));
                        placed = &curSignal->args.back();
                    }
                    // If arg tag is not self-closing, it may contain annotations
                    if (!isSelfClosing && placed)
                        curArg = placed;
                } else if (tagName == "annotation") {
                    std::string aname = extractAttr(tagContent, "name");
                    std::string aval  = extractAttr(tagContent, "value");
                    if (!aname.empty()) {
                        // Attach to innermost open container
                        if (curArg)          curArg->annotations[aname] = aval;
                        else if (curMethod)  curMethod->annotations[aname] = aval;
                        else if (curSignal)  curSignal->annotations[aname] = aval;
                        else if (curProperty) curProperty->annotations[aname] = aval;
                        else if (curIface)   curIface->annotations[aname] = aval;
                    }
                }

                if (isSelfClosing) {
                    if (tagName == "interface") curIface = nullptr;
                    if (tagName == "arg") curArg = nullptr;
                    if (tagName == "property") curProperty = nullptr;
                }
            }

            pos = tagEnd + 1;
        }

        return node;
    }

} // namespace mbedbus
