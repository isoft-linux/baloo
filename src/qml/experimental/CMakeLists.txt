add_definitions(-DTRANSLATION_DOMAIN=\"baloomonitorplugin\")

set(DBUS_INTERFACES
  ${CMAKE_BINARY_DIR}/src/dbus/schedulerinterface.cpp
  ${CMAKE_BINARY_DIR}/src/dbus/fileindexerinterface.cpp
)

SET_SOURCE_FILES_PROPERTIES(${DBUS_INTERFACES} PROPERTIES GENERATED 1)

set(baloomonitorplugin_SRCS
    baloomonitorplugin.cpp
    monitor.cpp
    ${DBUS_INTERFACES}
)

add_library(baloomonitorplugin SHARED ${baloomonitorplugin_SRCS})
add_dependencies(baloomonitorplugin BalooDBusInterfaces)

target_link_libraries(baloomonitorplugin
    PUBLIC
    Qt5::Core
    Qt5::Qml
    Qt5::DBus
    KF5::I18n
    KF5::CoreAddons
    PRIVATE
    KF5::Baloo
    KF5::BalooEngine
)
install(TARGETS baloomonitorplugin DESTINATION ${QML_INSTALL_DIR}/org/kde/baloo/experimental)

install(FILES qmldir DESTINATION ${QML_INSTALL_DIR}/org/kde/baloo/experimental)
