#pragma once
#include "Equipment.h"
#include <vector>
#include <string>
#include <memory>

class SQLiteEquipmentRepository {
private:
    std::string dbPath;
public:
    explicit SQLiteEquipmentRepository(const std::string& db_path);

    void initialize();

    // Базові методи з лабораторної роботи №8
    int addEquipment(const Equipment& eq);
    std::vector<Equipment> getAllEquipment();
    void updateEquipment(const Equipment& eq);
    void deleteEquipment(int id);

    void addEquipmentWithMaintenance(const Equipment& eq, const Maintenance& maint);
    void printEquipmentWithMaintenance();

    // Нові методи для ЛР №10 (робота з виробами та історією станів)
    int addProduct(const Product& product);
    std::unique_ptr<Product> getProductById(int id);
    std::vector<Product> getAllProducts();

    // зміна стану виробу + запис в історію в одній транзакції SQLite
    bool updateProductStatusWithHistory(int productId,
        const std::string& oldStatus,
        const std::string& newStatus,
        const std::string& changedAt);

    std::vector<ProductStatusHistory> getProductHistory(int productId);
};