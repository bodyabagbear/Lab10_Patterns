#pragma once
#include <string>

// Сутність обладнання (з лабораторної роботи №8)
struct Equipment {
    int id;
    std::string name;
    std::string status;
    double temperature;
    double vibration_level;
    int uptime_hours;
};

// Сутність технічного обслуговування (з лабораторної роботи №8)
struct Maintenance {
    int id;
    int equipmentId;
    std::string date;
    std::string description;
};

// Нова залежна сутність для ЛР №10: виріб (деталь), виготовлений на обладнанні
struct Product {
    int id;
    int equipmentId;        // Посилання на верстат/лінію (Foreign Key)
    std::string name;       // Назва деталі
    std::string status;     // Стан: Manufactured, Inspected, Accepted, Defective
    double actualValue;     // Фактичне значення параметра
    double standardValue;   // Нормативне значення параметра
    double tolerance;       // Допустиме відхилення
};

// Історія переходів станів виробу (зберігається в БД для патерну State)
struct ProductStatusHistory {
    int id;
    int productId;          // Посилання на виріб (Foreign Key)
    std::string oldStatus;  // Попередній стан
    std::string newStatus;  // Новий стан
    std::string changedAt;  // Час зміни стану
};