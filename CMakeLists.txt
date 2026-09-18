cmake_minimum_required(VERSION 3.24)

project(PlainwireDesktop VERSION 2.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# build.sh explicitly configures before every build. Suppressing Ninja's
# automatic CMake regeneration avoids dirty-manifest loops on skewed mtimes.
set(CMAKE_SUPPRESS_REGENERATION ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
  Core
  Widgets
  Network
  WebEngineCore
  WebEngineWidgets
)

qt_standard_project_setup()

qt_add_executable(plainwire-desktop
  src/main.cpp
  src/main_window.cpp
  src/plainwire_page.cpp
  src/single_instance.cpp
  src/url_policy.cpp
)

qt_add_resources(plainwire-desktop plainwire_resources
  PREFIX "/plainwire"
  FILES
    resources/plainwire.png
    resources/desktop-polish.css
)

target_compile_definitions(plainwire-desktop PRIVATE
  PLAINWIRE_VERSION="${PROJECT_VERSION}"
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
)

target_include_directories(plainwire-desktop PRIVATE src)

target_link_libraries(plainwire-desktop PRIVATE
  Qt6::Widgets
  Qt6::Network
  Qt6::WebEngineCore
  Qt6::WebEngineWidgets
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(plainwire-desktop PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
  )
endif()


enable_testing()

qt_add_executable(plainwire-url-policy-tests
  tests/url_policy_test.cpp
  src/url_policy.cpp
)
target_include_directories(plainwire-url-policy-tests PRIVATE src)
target_link_libraries(plainwire-url-policy-tests PRIVATE Qt6::Core)
target_compile_definitions(plainwire-url-policy-tests PRIVATE
  PLAINWIRE_VERSION="${PROJECT_VERSION}"
  QT_NO_CAST_FROM_ASCII
  QT_NO_CAST_TO_ASCII
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(plainwire-url-policy-tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
  )
endif()
add_test(NAME plainwire-url-policy COMMAND plainwire-url-policy-tests)

install(TARGETS plainwire-desktop RUNTIME DESTINATION bin)
install(FILES resources/plainwire.png
  DESTINATION share/icons/hicolor/256x256/apps
  RENAME plainwire.png)
install(FILES packaging/me.kokonico.plainwire.desktop
  DESTINATION share/applications)
