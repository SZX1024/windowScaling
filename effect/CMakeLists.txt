kcoreaddons_add_plugin(kwin4_effect_magpie2
    SOURCES
        magpieeffect.cpp
        magpieeffect.h
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

target_include_directories(kwin4_effect_magpie2 PRIVATE
    /usr/include/kwin
)

target_link_libraries(kwin4_effect_magpie2 PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::GlobalAccel
    KF6::I18n
    KF6::WindowSystem
    KWin::kwin
)
