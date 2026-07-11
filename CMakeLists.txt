cmake_minimum_required(VERSION 3.16)
project(index_fund_monitor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_library(SQLITE3_LIBRARY
    NAMES sqlite3 libsqlite3 libsqlite3.dll
    PATHS "C:/msys64/ucrt64/lib"
    REQUIRED
)

add_executable(index_fund_monitor
    src/main.cpp
    src/cli/parser.cpp
    src/db/database.cpp
    src/db/schema.cpp
    src/db/fund_repository.cpp
    src/models/models.cpp
    src/models/validation.cpp
)

target_include_directories(index_fund_monitor PRIVATE include)

target_link_libraries(index_fund_monitor PRIVATE ${SQLITE3_LIBRARY})

target_compile_options(index_fund_monitor PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor
)

function(add_test_target NAME)
    add_executable(${NAME} ${ARGN})
    target_include_directories(${NAME} PRIVATE include)
    target_compile_options(${NAME} PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion
        -Wshadow -Wnon-virtual-dtor
    )
endfunction()

add_test_target(test_parser
    tests/test_parser.cpp
    src/cli/parser.cpp
)

add_test_target(test_models
    tests/test_models.cpp
    src/models/models.cpp
    src/models/validation.cpp
)

add_test_target(test_database
    tests/test_database.cpp
    src/db/database.cpp
    src/db/schema.cpp
    src/models/models.cpp
)

target_link_libraries(test_database PRIVATE ${SQLITE3_LIBRARY})

add_test_target(test_fund_repository
    tests/test_fund_repository.cpp
    src/db/database.cpp
    src/db/schema.cpp
    src/db/fund_repository.cpp
    src/models/models.cpp
)

target_link_libraries(test_fund_repository PRIVATE ${SQLITE3_LIBRARY})

add_test_target(test_data_providers
    tests/test_data_providers.cpp
    src/data/mock_fund_directory_provider.cpp
    src/data/mock_index_valuation_provider.cpp
    src/data/mock_fund_distribution_provider.cpp
    src/data/mock_price_provider.cpp
    src/models/models.cpp
)

target_include_directories(test_data_providers PRIVATE "C:/msys64/ucrt64/include")

find_library(CURL_LIBRARY
    NAMES libcurl curl libcurl.dll
    PATHS "C:/msys64/ucrt64/lib"
    REQUIRED
)

add_test_target(test_http_client
    tests/test_http_client.cpp
    src/net/http_client.cpp
    src/net/mock_http_client.cpp
)

target_link_libraries(test_http_client PRIVATE ${CURL_LIBRARY})

target_include_directories(test_http_client PRIVATE "C:/msys64/ucrt64/include")

add_test_target(test_util
    tests/test_util.cpp
    src/util/json_reader.cpp
    src/util/csv_reader.cpp
)

target_include_directories(test_util PRIVATE "C:/msys64/ucrt64/include")