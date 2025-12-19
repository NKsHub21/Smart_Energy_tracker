#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "json.hpp"

using json = nlohmann::json;

const int DAYS_IN_WEEK = 7;
const double DAILY_THRESHOLD_KWH = 50.0;

class ReportConfig {
private:
    double costPerKWh_ = 0.0;
    double weeklyGoalKWh_ = 0.0;
    double co2Factor_ = 0.0;
    double costPerKgCO2_ = 0.0;

public:
    ReportConfig(double cost, double goal, double co2, double carbon_cost)
        : costPerKWh_(cost), weeklyGoalKWh_(goal), co2Factor_(co2), costPerKgCO2_(carbon_cost) {}

    double getCostPerKWh() const { return costPerKWh_; }
    double getWeeklyGoalKWh() const { return weeklyGoalKWh_; }
    double getCo2Factor() const { return co2Factor_; }
    double getCostPerKgCO2() const { return costPerKgCO2_; }
};

class Device {
private:
    std::string name_;
    double power_watts_ = 0.0;

public:
    Device(const std::string& name, double power)
        : name_(name), power_watts_(power) {}

    std::string getName() const { return name_; }
    double getPowerWatts() const { return power_watts_; }
};

double computeKWh(double power_watts, double hours_used) {
    return (power_watts * hours_used) / 1000.0;
}

void runCalculation() {
    json input_data;
    try {
        std::string line;
        std::string json_str;
        while (std::getline(std::cin, line)) {
            json_str += line;
        }
        input_data = json::parse(json_str);
    } catch (const std::exception& e) {
        json error_output;
        error_output["error"] = "Invalid JSON Input or Read Error";
        error_output["details"] = e.what();
        std::cout << error_output.dump(4) << std::endl;
        return;
    }

    ReportConfig config(
        input_data["configs"].value("costPerKWh", 0.0),
        input_data["configs"].value("weeklyEnergyGoal", 0.0),
        input_data["configs"].value("co2Factor", 0.0),
        input_data["configs"].value("costPerKgCO2", 0.0)
    );

    double costPerKWh = config.getCostPerKWh();
    double weeklyGoalKWh = config.getWeeklyGoalKWh();
    double co2Factor = config.getCo2Factor();
    double costPerKgCO2 = config.getCostPerKgCO2();

    const json& devices_json = input_data["devices"];
    const json& usageData = input_data["usage"];

    double overallWeeklyTotalKWh = 0.0;
    std::map<std::string, double> weeklyDeviceKWh;
    std::map<std::string, double> weeklyDeviceCost;
    std::vector<double> dailyTotalsKWh(DAYS_IN_WEEK, 0.0);
    std::vector<std::string> alertDays;

    json daily_reports_data = json::array();

    for (int day = 0; day < DAYS_IN_WEEK; ++day) {
        double dailyHouseholdTotalKWh = 0.0;
        double dailyCostTotal = 0.0;

        json device_breakdown = json::array();

        for (json::const_iterator it = devices_json.begin(); it != devices_json.end(); ++it) {
            const std::string name = it.key();
            const json& device_data = it.value();

            double power = device_data.value("power_watts", 0.0);
            Device current_device(name, power);

            double hours = 0.0;
            if (usageData.count(name) && usageData.at(name).is_array() && usageData.at(name).size() > day) {
                hours = usageData.at(name).at(day).get<double>();
            }

            double kwh = computeKWh(current_device.getPowerWatts(), hours);
            double cost = kwh * costPerKWh;

            dailyHouseholdTotalKWh += kwh;
            dailyCostTotal += cost;

            weeklyDeviceKWh[name] += kwh;
            weeklyDeviceCost[name] += cost;

            device_breakdown.push_back({
                {"device", current_device.getName()},
                {"power_w", current_device.getPowerWatts()},
                {"hours_h", hours},
                {"kwh", kwh},
                {"cost", cost}
            });
        }

        dailyTotalsKWh[day] = dailyHouseholdTotalKWh;
        overallWeeklyTotalKWh += dailyHouseholdTotalKWh;

        if (dailyHouseholdTotalKWh > DAILY_THRESHOLD_KWH) {
            alertDays.push_back("Day " + std::to_string(day + 1));
        }

        json day_details;
        day_details["day_number"] = day + 1;
        day_details["dailyHouseholdTotalKWh"] = dailyHouseholdTotalKWh;
        day_details["dailyCostTotal"] = dailyCostTotal;
        day_details["deviceBreakdown"] = device_breakdown;

        daily_reports_data.push_back(day_details);
    }

    double overallWeeklyCost = overallWeeklyTotalKWh * costPerKWh;
    double averageDailyTotalKWh = overallWeeklyTotalKWh / DAYS_IN_WEEK;
    double co2Footprint = overallWeeklyTotalKWh * co2Factor;
    double co2Surcharge = co2Footprint * costPerKgCO2;

    json report_output;

    std::string goalStatusText = "N/A";
    if (weeklyGoalKWh > 0.0) {
        double overage = overallWeeklyTotalKWh - weeklyGoalKWh;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3);
        if (overage <= 0.0) {
            ss << "Saved " << std::abs(overage) << " kWh";
        } else {
            ss << "Over by " << overage << " kWh";
        }
        goalStatusText = ss.str();
    }

    report_output["overallWeeklyTotalKWh"] = overallWeeklyTotalKWh;
    report_output["averageDailyTotalKWh"] = averageDailyTotalKWh;
    report_output["weeklyCostTotal"] = overallWeeklyCost;
    report_output["co2Footprint"] = co2Footprint;
    report_output["co2Surcharge"] = co2Surcharge;
    report_output["dailyTotalsKWh"] = dailyTotalsKWh;
    report_output["dailyBreakdown"] = daily_reports_data;

    json per_device_weekly = json::array();
    for (auto const& pair : weeklyDeviceKWh) {
        const std::string& name = pair.first;
        const double& kwh = pair.second;

        per_device_weekly.push_back({
            {"device", name},
            {"kwh", kwh},
            {"cost", weeklyDeviceCost.at(name)}
        });
    }
    report_output["perDeviceWeeklySummary"] = per_device_weekly;

    report_output["alertDays"] = alertDays;
    report_output["goalStatusText"] = goalStatusText;

    std::cout << report_output.dump(4) << std::endl;
}

int main() {
    runCalculation();
    return 0;
}
