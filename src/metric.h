#pragma once

#include <algorithm>
#include <iterator>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <codecvt>
#include <locale>
#include <sstream>

#include <Poco/Format.h>
#include <Poco/SharedPtr.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

#include "caches.h"

using namespace Poco::JSON;

uint32_t Dist(const std::wstring& left_input, const std::wstring& right_input) {
    std::vector<uint32_t> buffer_src, buffer_dst;
    const std::wstring& left = left_input.size() < right_input.size() ? left_input : right_input;
    const std::wstring& right = left_input.size() >= right_input.size() ? left_input : right_input;

    if (buffer_src.size() < left.size() + 1) {
        buffer_src.resize(left.size() + 1);
        buffer_dst.resize(left.size() + 1);
    }

    for (uint32_t i = 0; i < buffer_src.size(); ++i) {
        buffer_src[i] = i;
    }

    for (size_t i = 1; i < right.size() + 1; ++i) {
        for (size_t j = 0; j < left.size() + 1; ++j) {
            if (j == 0) {
                buffer_dst[j] = i;
            } else {
                uint32_t substitution_const = (left[j - 1] == right[i - 1]) ? 0 : 1;
                buffer_dst[j] = std::min(buffer_dst[j - 1] + 1, buffer_src[j] + 1);
                buffer_dst[j] = std::min(buffer_dst[j], substitution_const + buffer_src[j - 1]);
            }
        }
        buffer_dst.swap(buffer_src);
    }
    return buffer_src.back();
}

class AbstractWStringMetric {
public:
    virtual uint32_t operator()(const std::wstring& left, const std::wstring& right) = 0;
};


class LevensteinMetric : public AbstractWStringMetric {
public:
    LevensteinMetric() = default;
    uint32_t operator()(const std::wstring& left_input, const std::wstring& right_input) override;

private:
    std::vector<uint32_t> buffer_src, buffer_dst;
};


namespace hashes {
    template<typename T>
    struct hash {};

    template<>
    struct hash<wchar_t> {
        std::size_t operator()(wchar_t k) const {
            return static_cast<std::size_t>(k);
        }
    };

    template<>
    struct hash<std::pair<wchar_t, wchar_t>> {
        std::size_t operator()(const std::pair<wchar_t, wchar_t>& k) const {
            size_t h1 = hash<wchar_t>()(k.first);
            size_t h2 = hash<wchar_t>()(k.second);
            return (h1 << 2) ^ h2;
        }
    };
}

class WeightedLevensteinMetric : public AbstractWStringMetric {
public:
    WeightedLevensteinMetric();
    explicit WeightedLevensteinMetric(const std::string& config_file_name);
    uint32_t operator()(const std::wstring& left_input, const std::wstring& right_input) override;

private:
    uint32_t get_insert_delete_cost(wchar_t ch) const;
    uint32_t get_replace_cost(wchar_t first, wchar_t second);

    std::vector<uint32_t> buffer_src, buffer_dst;
    uint32_t default_insert_delete_ = 1;
    uint32_t default_replace_ = 1;
    std::unordered_map<wchar_t, uint32_t, hashes::hash<wchar_t>> insert_delete_costs_;
    std::unordered_map<std::pair<wchar_t, wchar_t>, uint32_t,
        hashes::hash<std::pair<wchar_t, wchar_t>>> replace_costs_;
    bool is_case_sensitive_ = true;
    BloomCache<wchar_t, hashes::hash<wchar_t>> insert_delete_cache_;
    BloomCache<std::pair<wchar_t, wchar_t>, hashes::hash<std::pair<wchar_t, wchar_t>>> replace_cache_;
};


uint32_t LevensteinMetric::operator()(const std::wstring& left_input, const std::wstring& right_input) {
    const std::wstring& left = left_input.size() < right_input.size() ? left_input : right_input;
    const std::wstring& right = left_input.size() >= right_input.size() ? left_input : right_input;

    if (buffer_src.size() < left.size() + 1) {
        buffer_src.resize(left.size() + 1);
        buffer_dst.resize(left.size() + 1);
    }

    for (uint32_t i = 0; i < buffer_src.size(); ++i) {
        buffer_src[i] = i;
    }

    for (size_t i = 1; i < right.size() + 1; ++i) {
        for (size_t j = 0; j < left.size() + 1; ++j) {
            if (j == 0) {
                buffer_dst[j] = i;
            } else {
                uint32_t substitution_const = (left[j - 1] == right[i - 1]) ? 0 : 1;
                buffer_dst[j] = std::min(buffer_dst[j - 1] + 1, buffer_src[j] + 1);
                buffer_dst[j] = std::min(buffer_dst[j], substitution_const + buffer_src[j - 1]);
            }
        }
        buffer_dst.swap(buffer_src);
    }
    return buffer_src[left.size()];
}


WeightedLevensteinMetric::WeightedLevensteinMetric()
        : AbstractWStringMetric(), default_insert_delete_(1), default_replace_(1) {
}

WeightedLevensteinMetric::WeightedLevensteinMetric(const std::string& config_file_name)
        : AbstractWStringMetric(), default_insert_delete_(1), default_replace_(1) {
    try {
        std::ifstream config(config_file_name);
        if (!config) {
            throw std::runtime_error(Poco::format("Metric config file \"%s\" can't be opened", config_file_name));
        }

        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        Parser parser;
        Object::Ptr config_object = parser.parse(config).extract<Object::Ptr>();
        // parse default section
        auto default_config = config_object->getObject("default");
        default_insert_delete_ = default_config->getValue<uint32_t>("insert_delete");
        default_replace_ = default_config->getValue<uint32_t>("replace");
        try {
            is_case_sensitive_ = default_config->getValue<bool>("case_sensitive");
        } catch (...) {
            ;
        }

        // parse insert-delete section
        auto insert_delete_array = config_object->getArray("custom_insert_delete");
        for (size_t index = 0; index < insert_delete_array->size(); ++index) {
            auto object = insert_delete_array->getObject(index);
            try {
                auto group_bytes = object->getValue<std::string>("group");
                std::wstring group = converter.from_bytes(group_bytes);
                if (!is_case_sensitive_) {
                    std::transform(group.begin(), group.end(), group.begin(), towlower);
                }

                auto cost = object->getValue<uint32_t>("cost");

                for (const auto& elem: group) {
                    insert_delete_costs_[elem] = cost;
                    insert_delete_cache_.Add(elem);
                }
            } catch (std::exception& e) {
                std::ostringstream log;
                log << Poco::format("Error while parsing custom insert-delete section %z:\n", index);
                object->stringify(log, 4);
                log << std::endl << e.what() << std::endl;
                throw std::runtime_error(log.str());
            }
        }

        // parse replace section
        auto replace_array = config_object->getArray("custom_replace");
        for (size_t index = 0; index < insert_delete_array->size(); ++index) {
            auto object = replace_array->getObject(index);
            try {
                auto first_group_bytes = object->getValue<std::string>("first_group");
                std::wstring first_group = converter.from_bytes(first_group_bytes);
                auto second_group_bytes = object->getValue<std::string>("second_group");
                std::wstring second_group = converter.from_bytes(second_group_bytes);
                auto cost = object->getValue<uint32_t>("cost");
                if (!is_case_sensitive_) {
                    std::transform(first_group.begin(), first_group.end(), first_group.begin(), towlower);
                    std::transform(second_group.begin(), second_group.end(), second_group.begin(), towlower);
                }
                for (const auto& first: first_group) {
                    for (const auto& second: second_group) {
                        replace_costs_[std::make_pair(first, second)] = cost;
                        replace_costs_[std::make_pair(second, first)] = cost;
                        replace_cache_.Add(std::make_pair(first, second));
                        replace_cache_.Add(std::make_pair(second, first));
                    }
                }
            } catch (std::exception& e) {
                std::ostringstream log;
                log << Poco::format("Error while parsing custom replace section %z:\n", index);
                object->stringify(log, 4);
                log << std::endl << e.what() << std::endl;
                throw std::runtime_error(log.str());
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Can't parse metric config file" << std::endl;
        throw e;
    }
}

uint32_t WeightedLevensteinMetric::get_insert_delete_cost(wchar_t ch) const {
    if (!is_case_sensitive_) {
        ch = towlower(ch);
    }
    if (!insert_delete_cache_.Check(ch)) {
        return default_insert_delete_;
    }
    auto search = insert_delete_costs_.find(ch);
    if (search != insert_delete_costs_.end()) {
        return search->second;
    } else {
        return default_insert_delete_;
    }
}

uint32_t WeightedLevensteinMetric::get_replace_cost(wchar_t first, wchar_t second) {
    if (!is_case_sensitive_) {
        first = towlower(first);
        second = towlower(second);
    }
    if (first == second) {
        return 0;
    }

    if (!replace_cache_.Check(std::make_pair(first, second))) {
        return default_replace_;
    }
    auto search = replace_costs_.find(std::make_pair(first, second));
    if (search != replace_costs_.end()) {
        return search->second;
    } else {
        return default_replace_;
    }
}

uint32_t WeightedLevensteinMetric::operator()(const std::wstring& left_input, const std::wstring& right_input) {
    const std::wstring& left = left_input.size() < right_input.size() ? left_input : right_input;
    const std::wstring& right = left_input.size() >= right_input.size() ? left_input : right_input;

    if (buffer_src.size() < left.size() + 1) {
        buffer_src.resize(left.size() + 1);
        buffer_dst.resize(left.size() + 1);
    }

    for (uint32_t i = 0; i < left.size() + 1; ++i) {
        buffer_src[i] = i;
    }

    for (size_t i = 1; i < right.size() + 1; ++i) {
        for (size_t j = 0; j < left.size() + 1; ++j) {
            if (j == 0) {
                buffer_dst[j] = i;
            } else {
                uint32_t substitution = buffer_src[j - 1] + get_replace_cost(left[i - 1], right[j - 1]);
                uint32_t right_insert = buffer_src[j] + get_insert_delete_cost(right[j - 1]);
                uint32_t left_insert = buffer_dst[j - 1] + get_insert_delete_cost(left[i - 1]);

                buffer_dst[j] = std::min(right_insert, left_insert);
                buffer_dst[j] = std::min(buffer_dst[j], substitution);
            }
        }
        buffer_dst.swap(buffer_src);
    }
    return buffer_src[left.size()];
}
