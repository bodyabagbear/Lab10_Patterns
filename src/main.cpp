#include <iostream>
#include <iomanip>
#include <memory>
#include <cassert>
#include <cmath>
#include <filesystem>
#include "../include/SQLiteEquipmentRepository.h"
#include "../include/ProductionLineFacade.h"

// Вивід історії переходів станів виробу з бази даних
void printHistoryFromDB(SQLiteEquipmentRepository& repo, int productId) {
    auto history = repo.getProductHistory(productId);
    std::cout << "  DB History for Product #" << productId << ":" << std::endl;
    for (const auto& h : history) {
        std::cout << "    [" << h.changedAt << "] " << h.oldStatus << " -> " << h.newStatus << std::endl;
    }
}

// Модульні тести (перевірка State, Strategy, Observer, Facade та транзакцій)
void runUnitTests() {
    std::cout << "\n RUNNING UNIT TESTS " << std::endl;
    const std::string testDb = "test_factory_lab10.sqlite";
    std::filesystem::remove(testDb);

    SQLiteEquipmentRepository repo(testDb);
    repo.initialize();
    int eqId = repo.addEquipment(Equipment{ -1, "Test CNC Machine", "Active", 40.0, 1.2, 50 });

    // тест 1: Патерн Strategy (усі 3 алгоритми + заміна в runtime + обробка помилок)
    {
        DeviationEvaluator eval(std::make_unique<AbsoluteDeviationStrategy>());
        assert(std::fabs(eval.evaluate(50.5, 50.0, 1.0) - 0.5) < 1e-6);

        eval.setStrategy(std::make_unique<RelativeDeviationStrategy>());
        assert(std::fabs(eval.evaluate(50.5, 50.0, 1.0) - 1.0) < 1e-6);

        eval.setStrategy(std::make_unique<NormalizedDeviationStrategy>());
        assert(std::fabs(eval.evaluate(50.5, 50.0, 1.0) - 0.5) < 1e-6);

        // Перевірка обробки помилки (ділення на нульовий допуск)
        bool exceptionCaught = false;
        try {
            eval.evaluate(50.5, 50.0, 0.0);
        }
        catch (const std::invalid_argument&) {
            exceptionCaught = true;
        }
        assert(exceptionCaught);
        std::cout << "[PASS] Test 1: Strategy algorithms, switching & error handling" << std::endl;
    }

    // Тест 2: Патерн State (допустимі/заборонені переходи + атомарна історія в БД)
    {
        Product p{ -1, eqId, "Test Bolt", "Manufactured", 10.1, 10.0, 0.2 };
        p.id = repo.addProduct(p);
        ProductContext ctx(p, repo, std::make_unique<ManufacturedState>());

        // Недопустимий перехід: не можна прийняти до перевірки
        assert(ctx.accept() == false);
        assert(ctx.getStateName() == "Manufactured");

        // Допустимі переходи: Manufactured -> Inspected -> Accepted
        assert(ctx.inspect() == true);
        assert(ctx.getStateName() == "Inspected");
        assert(ctx.accept() == true);
        assert(ctx.getStateName() == "Accepted");

        // Недопустимий перехід із фінального стану
        assert(ctx.markDefective() == false);

        // Перевірка збереження в БД та історії переходів
        auto dbProduct = repo.getProductById(p.id);
        assert(dbProduct != nullptr && dbProduct->status == "Accepted");

        auto history = repo.getProductHistory(p.id);
        assert(history.size() == 2);
        assert(history[0].oldStatus == "Manufactured" && history[0].newStatus == "Inspected");
        assert(history[1].oldStatus == "Inspected" && history[1].newStatus == "Accepted");
        std::cout << "[PASS] Test 2: State transitions, blocking & SQLite transaction history" << std::endl;
    }

    // Тест 3: Патерн Observer (підписка, сповіщення, відписка, weak_ptr очищення)
    {
        ProductionEventManager manager;
        auto journal = std::make_shared<ProductionJournal>();
        auto stats = std::make_shared<QualityStatistics>();

        manager.subscribe(journal);
        manager.subscribe(stats);
        assert(manager.getActiveObserversCount() == 2);

        ProductionEvent ev1{ ProductionEventType::PartManufactured, 1, "PartA", "Manufactured", 0.0, "" };
        manager.notify(ev1);
        assert(journal->getLogs().size() == 1);
        assert(stats->getManufacturedCount() == 1);

        // Відписуємо журнал, і подію має отримати лише статистика
        manager.unsubscribe(journal);
        assert(manager.getActiveObserversCount() == 1);

        ProductionEvent ev2{ ProductionEventType::QualityControlCompleted, 1, "PartA", "Defective", 2.5, "Normalized" };
        manager.notify(ev2);
        assert(journal->getLogs().size() == 1);      // Журнал не змінився
        assert(stats->getDefectiveCount() == 1);    // Статистика оновилася

        // Перевірка автоматичного очищення знищеного підписника через weak_ptr
        stats.reset();
        assert(manager.getActiveObserversCount() == 0);
        std::cout << "[PASS] Test 3: Observer subscribe, unsubscribe & weak_ptr lifecycle" << std::endl;
    }

    // тест 4: патерн Facade (повний цикл виготовлення та перевірки хорошої і бракованої деталі) 
    {
        ProductionLineFacade facade(repo, std::make_unique<NormalizedDeviationStrategy>());
        auto stats = std::make_shared<QualityStatistics>();
        facade.subscribe(stats);

        auto goodPart = facade.producePart(eqId, "Good Shaft", 100.1, 100.0, 0.5);
        double devGood = facade.inspectAndEvaluate(*goodPart);
        assert(goodPart->getStateName() == "Accepted");
        assert(std::fabs(devGood - 0.2) < 1e-6);

        auto badPart = facade.producePart(eqId, "Bad Shaft", 101.5, 100.0, 0.5);
        double devBad = facade.inspectAndEvaluate(*badPart);
        assert(badPart->getStateName() == "Defective");
        assert(std::fabs(devBad - 3.0) < 1e-6);

        assert(stats->getManufacturedCount() == 2);
        assert(stats->getAcceptedCount() == 1);
        assert(stats->getDefectiveCount() == 1);
        std::cout << "[PASS] Test 4: Facade end-to-end integration" << std::endl;
    }

    std::filesystem::remove(testDb);
    std::cout << " ALL 4 UNIT TESTS PASSED! \n" << std::endl;
}

// ГОЛОВНА ФУНКЦІЯ: ДЕМОНСТРАЦІЯ РОБОТИ СИСТЕМИ
int main() {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << " Production Line System (Lab 8 ORM + Lab 10 Patterns) \n" << std::endl;

    const std::string dbFile = "factory_lab10.sqlite";
    std::filesystem::remove(dbFile); 

    // 1. базова функціональність ЛР №8 (Equipment + Maintenance + SQLite ORM)
    SQLiteEquipmentRepository repo(dbFile);
    repo.initialize();

    Equipment eq1{ -1, "Robotic Arm X1", "Active", 45.5, 2.1, 120 };
    int eqId1 = repo.addEquipment(eq1);
    std::cout << "[Lab 8] Added equipment ID: " << eqId1 << " (" << eq1.name << ")" << std::endl;

    Equipment eq2{ -1, "Conveyor Belt B", "Maintenance", 55.0, 4.5, 300 };
    Maintenance maint{ -1, 0, "2026-11-20", "Replaced worn out motor bearings" };
    repo.addEquipmentWithMaintenance(eq2, maint);
    repo.printEquipmentWithMaintenance();

    // 2. Ініціалізація Фасаду (Facade) та підключення підписників (Observer)
    ProductionLineFacade facade(repo, std::make_unique<AbsoluteDeviationStrategy>());
    auto journal = std::make_shared<ProductionJournal>();
    auto statistics = std::make_shared<QualityStatistics>();

    facade.subscribe(journal);
    facade.subscribe(statistics);

    // 3. Сценарій 1: Виготовлення та успішне приймання деталі (Абсолютне відхилення)
    std::cout << " Scenario 1: Normal Part (Absolute Deviation Strategy) " << std::endl;
    auto part1 = facade.producePart(eqId1, "Steel Shaft A1", 50.20, 50.00, 0.50);

    // Демонстрація блокування некоректного переходу (приймання без перевірки)
    part1->accept();

    // Повна перевірка через Фасад: Manufactured -> Inspected -> Accepted
    facade.inspectAndEvaluate(*part1);
    printHistoryFromDB(repo, part1->getProduct().id);

    // 4. Сценарій 2: Заміна стратегії під час виконання на Відносне відхилення (%)
    std::cout << "\n Scenario 2: Switching Strategy to Relative Deviation (%) " << std::endl;
    facade.setDeviationStrategy(std::make_unique<RelativeDeviationStrategy>());

    auto part2 = facade.producePart(eqId1, "Bearing Ring B2", 100.30, 100.00, 0.50);
    facade.inspectAndEvaluate(*part2);
    printHistoryFromDB(repo, part2->getProduct().id);

    // 5. Сценарій 3: Заміна стратегії на Нормоване відхилення + відписка підписника + Брак
    std::cout << "\n Scenario 3: Defective Part (Normalized Strategy + Unsubscribe Journal) " << std::endl;
    facade.setDeviationStrategy(std::make_unique<NormalizedDeviationStrategy>());
    facade.unsubscribe(journal); // Відключаємо журнал — сповіщення отримає тільки статистика

    auto part3 = facade.producePart(eqId1, "Gear Wheel G4", 52.00, 50.00, 0.50);
    facade.inspectAndEvaluate(*part3);
    printHistoryFromDB(repo, part3->getProduct().id);

    // Вивід підсумкової статистики якості
    statistics->printSummary();

    // і 6. Запуск автоматичних модульних тестів
    runUnitTests();

    return 0;
}