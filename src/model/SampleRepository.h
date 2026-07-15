#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Sample.h"
#include "../persistence/JsonStore.h"

class SampleRepository {
public:
    struct RegisterResult {
        bool success;
        std::string errorMessage;
    };

    explicit SampleRepository(sampleorder::JsonStore& store);

    RegisterResult registerSample(const Sample& sample);
    std::optional<Sample> find(const std::string& id) const;
    std::vector<Sample> list() const;
    std::vector<Sample> search(const std::string& keyword) const;

private:
    sampleorder::JsonStore& store_;
};
