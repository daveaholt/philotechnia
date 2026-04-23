// Philotechnia — M0a placeholder entry point.
//
// This file exists to prove the full build → link → launch path on every
// target platform. It intentionally touches Qt — a #include of <QString>
// and a QString instantiation — so the Qt link is real rather than a
// dead dependency the linker could drop. Real application logic lands
// in M0b onwards as the storage, core, vcs, ui, and commands subsystems
// come online. See docs/philotechnia_spec.md §15.1 for the M0a
// deliverable detail.

#include <iostream>

#include <QString>
#include <QtGlobal>

#ifndef PHILOTECHNIA_VERSION
#define PHILOTECHNIA_VERSION "unknown"
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const QString qt_banner =
        QStringLiteral("Qt ") + QString::fromLatin1(qVersion());

    std::cout << "Philotechnia " << PHILOTECHNIA_VERSION
              << " (M0a stub — no application behaviour yet) ["
              << qt_banner.toStdString() << "]\n";
    return 0;
}
