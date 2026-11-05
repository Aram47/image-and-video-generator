cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0135 NEW)
project(cpp-image-video-generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

add_library(generator_core STATIC
  src/common/error.cpp
  src/common/logger.cpp
  src/common/metrics.cpp
  src/common/token_verifier.cpp
  src/common/utf8.cpp
  src/character/palette.cpp
  src/character/character_generator.cpp
  src/character/pose_library.cpp
  src/character/skeleton.cpp
  src/animation/action_library.cpp
  src/character/mesh_primitives.cpp
  src/character/mesh_builder.cpp
  src/character/hair_mesh_builder.cpp
  src/character/outfit_mesh_builder.cpp
  src/render/camera.cpp
  src/render/scene.cpp
  src/render/environment_builder.cpp
  src/render/framebuffer.cpp
  src/render/cpu_rasterizer.cpp
  src/media/image_writer.cpp
  src/media/video_encoder.cpp
  src/media/stb_image_write_impl.cpp
  src/prompt/vocabulary.cpp
  src/prompt/prompt_normalizer.cpp
  src/prompt/prompt_parser.cpp
  src/app/server_config.cpp
  src/app/application.cpp
  src/storage/memory_job_store.cpp
  src/storage/file_job_store.cpp
  src/storage/output_store.cpp
  src/storage/output_quota.cpp
  src/storage/frame_cleaner.cpp
  src/storage/process_lock.cpp
  src/jobs/job_queue.cpp
  src/jobs/worker.cpp
  src/jobs/pipeline_failure.cpp
  src/jobs/scene_composer.cpp
  src/jobs/image_pipeline.cpp
  src/jobs/video_pipeline.cpp
  src/jobs/stub_job_executor.cpp
  src/jobs/generation_executor.cpp
  src/jobs/job_manager.cpp
  src/net/ndjson_framer.cpp
  src/net/protocol.cpp
  src/net/socket.cpp
  src/net/socket_stream.cpp
  src/net/tls_stream.cpp
  src/net/tls_context.cpp
  src/net/request_dispatcher.cpp
  src/net/connection.cpp
  src/net/tcp_server.cpp
)
target_include_directories(generator_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
)
target_link_libraries(generator_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(generator_core PRIVATE -Wall -Wextra -Wpedantic -g)

option(GEN_ENABLE_FFMPEG "Encode MP4 with libav" ON)
set(GEN_FFMPEG_FOUND OFF)
if(GEN_ENABLE_FFMPEG)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBAV QUIET IMPORTED_TARGET libavcodec libavformat libavutil libswscale)
  endif()
  if(TARGET PkgConfig::LIBAV)
    set(GEN_FFMPEG_FOUND ON)
  endif()
endif()
if(GEN_FFMPEG_FOUND)
  target_sources(generator_core PRIVATE src/media/libav_video_encoder.cpp src/media/stb_image_impl.cpp)
  target_compile_definitions(generator_core PUBLIC GEN_HAS_FFMPEG=1)
  target_link_libraries(generator_core PUBLIC PkgConfig::LIBAV)
  set_source_files_properties(src/media/stb_image_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
else()
  target_compile_definitions(generator_core PUBLIC GEN_HAS_FFMPEG=0)
  message(STATUS "libav not found; video encoding uses NullVideoEncoder")
endif()

add_executable(generator_server src/app/main.cpp)
target_link_libraries(generator_server PRIVATE generator_core)
target_compile_options(generator_server PRIVATE -Wall -Wextra -Wpedantic -g)

add_executable(generator_cli
  tools/cli/main.cpp
  tools/cli/cli_parser.cpp
  tools/cli/ndjson_client.cpp
)
target_include_directories(generator_cli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tools/cli")
target_link_libraries(generator_cli PRIVATE nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(generator_cli PRIVATE -Wall -Wextra -Wpedantic -g)

enable_testing()
add_executable(generator_tests
  tests/ndjson_framer_test.cpp
  tests/protocol_test.cpp
  tests/job_manager_test.cpp
  tests/server_config_test.cpp
  tests/server_integration_test.cpp
  tests/prompt_parser_test.cpp
  tests/cpu_rasterizer_test.cpp
  tests/character_generator_test.cpp
  tests/mesh_builder_test.cpp
  tests/scene_variant_test.cpp
  tests/png_image.cpp
  tests/image_writer_test.cpp
  tests/image_pipeline_test.cpp
  tests/animation_test.cpp
  tests/video_pipeline_test.cpp
  tests/deployment_test.cpp
)
target_link_libraries(generator_tests PRIVATE generator_core GTest::gtest_main)
target_compile_options(generator_tests PRIVATE -Wall -Wextra -Wpedantic -g)
add_dependencies(generator_tests generator_cli generator_server)
target_include_directories(generator_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/cli_path.hpp" CONTENT
"#pragma once
#define GEN_CLI_PATH \"$<TARGET_FILE:generator_cli>\"
#define GEN_SERVER_PATH \"$<TARGET_FILE:generator_server>\"
#define GEN_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\"
")
set_source_files_properties(
  src/media/stb_image_write_impl.cpp
  tests/png_image.cpp
  PROPERTIES COMPILE_OPTIONS "-w"
)
# AddressSanitizer deadlocks in Apple Clang 17 during process startup on this OS,
# so the default is UBSan only. GEN_ENABLE_ASAN turns AddressSanitizer back on.
option(GEN_ENABLE_ASAN "Link AddressSanitizer into the server and tests" OFF)
set(GEN_SANITIZER_FLAGS -fsanitize=undefined)
if(GEN_ENABLE_ASAN)
  list(APPEND GEN_SANITIZER_FLAGS -fsanitize=address)
endif()
target_compile_options(generator_core PRIVATE ${GEN_SANITIZER_FLAGS})
target_compile_options(generator_tests PRIVATE ${GEN_SANITIZER_FLAGS})
target_link_options(generator_core PUBLIC ${GEN_SANITIZER_FLAGS})
target_link_options(generator_tests PRIVATE ${GEN_SANITIZER_FLAGS})

include(GoogleTest)
gtest_discover_tests(generator_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 30)
