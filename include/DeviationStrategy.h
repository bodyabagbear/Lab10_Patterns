#pragma once
#include <cmath>
#include <memory>
#include <string>
#include <stdexcept>

// Абстрактний інтерфейс стратегії розрахунку відхилення параметра виробу
class IDeviationStrategy {
public:
    virtual ~IDeviationStrategy() = default;
    virtual double calculate(double actualValue, double standardValue, double tolerance) const = 0;
    virtual std::string getName() const = 0;
};

// Алгоритм 1: Абсолютне відхилення |фактичне - нормативне|
class AbsoluteDeviationStrategy : public IDeviationStrategy {
public:
    double calculate(double actualValue, double standardValue, double /*tolerance*/) const override {
        return std::fabs(actualValue - standardValue);
    }

    std::string getName() const override {
        return "Absolute Deviation";
    }
};

// Алгоритм 2: Відносне відхилення у відсотках (|факт - норма| / |норма|) * 100%
class RelativeDeviationStrategy : public IDeviationStrategy {
public:
    double calculate(double actualValue, double standardValue, double /*tolerance*/) const override {
        if (std::fabs(standardValue) < 1e-9) {
            throw std::invalid_argument("Standard value cannot be zero for relative deviation.");
        }
        return (std::fabs(actualValue - standardValue) / std::fabs(standardValue)) * 100.0;
    }

    std::string getName() const override {
        return "Relative Deviation (%)";
    }
};

// Алгоритм 3: Нормоване відхилення |факт - норма| / допустиме відхилення
class NormalizedDeviationStrategy : public IDeviationStrategy {
public:
    double calculate(double actualValue, double standardValue, double tolerance) const override {
        if (tolerance <= 0.0) {
            throw std::invalid_argument("Tolerance must be greater than zero.");
        }
        return std::fabs(actualValue - standardValue) / tolerance;
    }

    std::string getName() const override {
        return "Normalized Deviation (ratio to tolerance)";
    }
};

// Контекст для патерну Strategy (володіє стратегією через std::unique_ptr)
class DeviationEvaluator {
private:
    std::unique_ptr<IDeviationStrategy> strategy;

public:
    explicit DeviationEvaluator(std::unique_ptr<IDeviationStrategy> initialStrategy)
        : strategy(std::move(initialStrategy)) {
    }

    // Динамічна заміна алгоритму під час роботи програми
    void setStrategy(std::unique_ptr<IDeviationStrategy> newStrategy) {
        strategy = std::move(newStrategy);
    }

    double evaluate(double actualValue, double standardValue, double tolerance) const {
        if (!strategy) {
            throw std::runtime_error("Deviation strategy is not set.");
        }
        return strategy->calculate(actualValue, standardValue, tolerance);
    }

    std::string getStrategyName() const {
        return strategy ? strategy->getName() : "None";
    }
};