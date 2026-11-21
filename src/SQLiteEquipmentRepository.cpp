#include "../include/SQLiteEquipmentRepository.h"
#include <sqlite_orm/sqlite_orm.h>
#include <iostream>

using namespace sqlite_orm;

// Опис схеми бази даних (ORM-мапінг усіх 4 таблиць)
inline auto createStorage(const std::string& path) {
    return make_storage(path,
        make_table("equipment",
            make_column("id", &Equipment::id, autoincrement(), primary_key()),
            make_column("name", &Equipment::name),
            make_column("status", &Equipment::status),
            make_column("temperature", &Equipment::temperature),
            make_column("vibration_level", &Equipment::vibration_level),
            make_column("uptime_hours", &Equipment::uptime_hours)
        ),
        make_table("maintenance",
            make_column("id", &Maintenance::id, autoincrement(), primary_key()),
            make_column("equipment_id", &Maintenance::equipmentId),
            make_column("date", &Maintenance::date),
            make_column("description", &Maintenance::description),
            foreign_key(&Maintenance::equipmentId).references(&Equipment::id)
        ),
        make_table("products",
            make_column("id", &Product::id, autoincrement(), primary_key()),
            make_column("equipment_id", &Product::equipmentId),
            make_column("name", &Product::name),
            make_column("status", &Product::status),
            make_column("actual_value", &Product::actualValue),
            make_column("standard_value", &Product::standardValue),
            make_column("tolerance", &Product::tolerance),
            foreign_key(&Product::equipmentId).references(&Equipment::id)
        ),
        make_table("product_status_history",
            make_column("id", &ProductStatusHistory::id, autoincrement(), primary_key()),
            make_column("product_id", &ProductStatusHistory::productId),
            make_column("old_status", &ProductStatusHistory::oldStatus),
            make_column("new_status", &ProductStatusHistory::newStatus),
            make_column("changed_at", &ProductStatusHistory::changedAt),
            foreign_key(&ProductStatusHistory::productId).references(&Product::id)
        )
    );
}

SQLiteEquipmentRepository::SQLiteEquipmentRepository(const std::string& db_path)
    : dbPath(db_path) {
}

void SQLiteEquipmentRepository::initialize() {
    auto storage = createStorage(dbPath);
    storage.sync_schema();
}

int SQLiteEquipmentRepository::addEquipment(const Equipment& eq) {
    auto storage = createStorage(dbPath);
    return storage.insert(eq);
}

std::vector<Equipment> SQLiteEquipmentRepository::getAllEquipment() {
    auto storage = createStorage(dbPath);
    return storage.get_all<Equipment>();
}

void SQLiteEquipmentRepository::updateEquipment(const Equipment& eq) {
    auto storage = createStorage(dbPath);
    storage.update(eq);
}

void SQLiteEquipmentRepository::deleteEquipment(int id) {
    auto storage = createStorage(dbPath);
    storage.remove<Equipment>(id);
}

void SQLiteEquipmentRepository::addEquipmentWithMaintenance(const Equipment& eq, const Maintenance& maint) {
    auto storage = createStorage(dbPath);
    storage.transaction([&]() mutable {
        int eqId = storage.insert(eq);
        Maintenance newMaint = maint;
        newMaint.equipmentId = eqId;
        storage.insert(newMaint);
        return true;
        });
}

void SQLiteEquipmentRepository::printEquipmentWithMaintenance() {
    auto storage = createStorage(dbPath);
    auto rows = storage.select(
        columns(&Equipment::name, &Maintenance::date, &Maintenance::description),
        inner_join<Maintenance>(on(c(&Equipment::id) == &Maintenance::equipmentId))
    );

    std::cout << "\n Maintenance Report (JOIN Query) " << std::endl;
    for (auto& row : rows) {
        std::cout << "Equipment: " << std::get<0>(row)
            << " | Date: " << std::get<1>(row)
            << " | Issue: " << std::get<2>(row) << std::endl;
    }
    std::cout << "----\n" << std::endl;
}

int SQLiteEquipmentRepository::addProduct(const Product& product) {
    auto storage = createStorage(dbPath);
    return storage.insert(product);
}

std::unique_ptr<Product> SQLiteEquipmentRepository::getProductById(int id) {
    auto storage = createStorage(dbPath);
    return storage.get_pointer<Product>(id);
}

std::vector<Product> SQLiteEquipmentRepository::getAllProducts() {
    auto storage = createStorage(dbPath);
    return storage.get_all<Product>();
}

bool SQLiteEquipmentRepository::updateProductStatusWithHistory(int productId,
    const std::string& oldStatus,
    const std::string& newStatus,
    const std::string& changedAt) {
    auto storage = createStorage(dbPath);

    // Виконуємо оновлення статусу та додавання запису в історію в одній транзакції
    return storage.transaction([&]() mutable {
        auto prod = storage.get_pointer<Product>(productId);
        if (!prod) {
            return false; // Якщо виріб не знайдено — робимо відкат транзакції
        }

        prod->status = newStatus;
        storage.update(*prod);

        ProductStatusHistory record{ -1, productId, oldStatus, newStatus, changedAt };
        storage.insert(record);
        return true; // Фіксація транзакції (COMMIT)
        });
}

std::vector<ProductStatusHistory> SQLiteEquipmentRepository::getProductHistory(int productId) {
    auto storage = createStorage(dbPath);
    return storage.get_all<ProductStatusHistory>(
        where(c(&ProductStatusHistory::productId) == productId)
    );
}