// SichttestIntegration.hpp — template: let Sichttest drive a Qt application.
//
// This header does not include any Sichttest header, so it may be included anywhere.
// Own one instance in your main window (member std::unique_ptr<SichttestIntegration>),
// created once the window has restored its start state. It ends with the window,
// before QApplication.

#pragma once

#include <QString>

#include <memory>

class MainWindow;   // your window; replace with whatever performs the actions

class SichttestIntegration
{
public:
    // Does nothing unless the application was started with --testing and the
    // Sichttest package was found at configure time.
    SichttestIntegration(MainWindow& window, const QString& version);
    ~SichttestIntegration();

    SichttestIntegration(const SichttestIntegration&) = delete;
    SichttestIntegration& operator=(const SichttestIntegration&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
