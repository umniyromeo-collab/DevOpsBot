#pragma once
#include <pqxx/pqxx>

#include "../status_checker/"


class IStorage() {

    virtual ~IStorage = default;
    virtual void store(const Che)

};

template<typename storelambda>
class Storage {

public:
    Storage(const std::string& connection_str){}

    void Store(checker_result& res){}

    ~Storage();

};
