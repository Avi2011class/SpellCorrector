#pragma once
#include <iterator>
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>
#include <map>
#include <random>
#include <unordered_map>
#include <fstream>

#include <Poco/String.h>
#include <Poco/Format.h>

#include "metric.h"


struct SearchResult {
    std::wstring result;
    uint32_t tolerance;
    uint32_t priority;
};


class TreeNode {
public:
    friend class BKTree;

    TreeNode()
        : data_(L""), priority_(1), max_dist_(0), min_dist_(std::numeric_limits<uint32_t>::max()) {
    }

    explicit TreeNode(std::wstring data, uint32_t priority=1)
        : data_(std::move(data)), priority_(priority), max_dist_(0), min_dist_(std::numeric_limits<uint32_t>::max()) {
    };

    bool Insert(const std::wstring& new_data, uint32_t priority, AbstractWStringMetric& metric) {
        uint32_t distance = metric(new_data, data_); // metric(new_data, data_);
        if (distance != 0) {
            if (childs_.find(distance) != childs_.end()) {
                return childs_[distance]->Insert(new_data, priority, metric);
            } else {
                max_dist_ = std::max(max_dist_, distance);
                min_dist_ = std::min(min_dist_, distance);
                childs_[distance] = std::make_shared<TreeNode>(new_data, priority);
                return true;
            }
        } else {
            priority_ += priority;
            return false;
        }
    }

    void FindSimilar(const std::wstring& data, uint32_t tolerance, std::vector<SearchResult>& results,
            AbstractWStringMetric& metric) const {
        uint32_t my_distance = metric(data, data_);
        if (my_distance <= tolerance) {
            results.emplace_back(SearchResult({data_, my_distance, priority_}));
        }
        uint32_t start = (my_distance < tolerance) ?
                min_dist_ : std::max(my_distance - tolerance, min_dist_);
        uint32_t end = std::min(my_distance + tolerance, max_dist_);
        for (uint32_t dist = start; dist <= end; ++dist) {
            auto child = childs_.find(dist);
            if (child != childs_.end()) {
                child->second->FindSimilar(data, tolerance, results, metric);
            }
        }
    }

private:
    std::wstring data_;
    uint32_t priority_;
    std::unordered_map<uint32_t, std::shared_ptr<TreeNode>> childs_;
    uint32_t max_dist_, min_dist_;
};


class BKTree {
public:
    BKTree() : metric_(std::make_shared<LevensteinMetric>()), root_(nullptr) {};
    BKTree(const std::string& dictionary_file_name, std::shared_ptr<AbstractWStringMetric> metric)
            : metric_(std::move(metric)), root_(nullptr) {
        std::wifstream input_file(dictionary_file_name);
        if (!input_file) {
            throw std::runtime_error(
                    Poco::format("Dictionary file \"%s\" can't be opened",
                                 dictionary_file_name));
        }
        input_file.imbue(std::locale(""));
        std::vector<std::pair<std::wstring, uint32_t>> words;
        std::wstring word;
        uint32_t priority;
        std::cerr << Poco::format("Reading dictionary from %s... ", dictionary_file_name);
        while (input_file >> word >> priority) {
            if (word.length() > 0) {
                word = Poco::toLower(word);
                words.emplace_back(word, priority);
            }
        }
        std::cerr << "Done!" << std::endl;
        input_file.close();

        std::random_device rd;
        std::mt19937 mt(rd());
        std::shuffle(words.begin(), words.end(), mt);
        size_t index = 0;
        for (const auto& [elem, priority]: words) {
            Insert(elem, priority);
            ++index;
            if (index % 1000 == 0) {
                std::wcerr << "\rBuilding bk_tree: " << index << "/" << words.size();
            }
        }
        std::cerr << "\rBuilding bk_tree: " << words.size() << "/" << words.size();
        std::cerr << " Done!" << std::endl;
    }

    bool Insert(const std::wstring& data, uint32_t priority=1) {
        if (root_ != nullptr) {
            return root_->Insert(data, priority, *metric_);
        } else {
            root_ = std::make_shared<TreeNode>(data, priority);
            return true;
        }
    }

    [[nodiscard]] std::vector<SearchResult> FindSimilar(const std::wstring& data, uint32_t tolerance) const {
        if (root_ == nullptr) {
            return {};
        }
        std::vector<SearchResult> result;
        root_->FindSimilar(data, tolerance, result, *metric_);
        std::sort(result.begin(), result.end(), [](const auto& _1, const auto& _2) -> bool {
            return _1.tolerance != _2.tolerance ? _1.tolerance < _2.tolerance : _1.priority > _2.priority;
        });
        return result;
    }

private:
    std::shared_ptr<AbstractWStringMetric> metric_;
    std::shared_ptr<TreeNode> root_;
};


