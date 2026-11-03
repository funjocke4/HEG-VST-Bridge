#include "../vst3wrapper/worker_process.h"

#include <chrono>
#include <iostream>
#include <thread>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2)
        return 2;

    heg::WorkerProcess worker;

    if (!worker.start(argv[1])) {
        std::cerr << "FEHLER: Worker startet nicht\n";
        return 1;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(500)
    );

    if (!worker.running()) {
        std::cerr << "FEHLER: Worker nicht aktiv\n";
        return 3;
    }

    std::cout << "ERFOLG: Worker automatisch gestartet\n";

    worker.stop();

    if (worker.running()) {
        std::cerr << "FEHLER: Worker noch aktiv\n";
        return 4;
    }

    std::cout << "ERFOLG: Worker beendet\n";
    return 0;
}
