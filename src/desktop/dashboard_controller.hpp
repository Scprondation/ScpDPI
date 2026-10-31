#pragma once

#include "scpdpi/config/config.hpp"
#include "scpdpi/engine/core_engine.hpp"
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QJsonObject>
#include <filesystem>
#include <array>

class QTimer;

class DashboardController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString coreStatus READ coreStatus NOTIFY changed)
    Q_PROPERTY(QString profile READ profile NOTIFY changed)
    Q_PROPERTY(QString vpnStatus READ vpnStatus NOTIFY changed)
    Q_PROPERTY(QString defaultRoute READ defaultRoute NOTIFY changed)
    Q_PROPERTY(QString lastUpdate READ lastUpdate NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
    Q_PROPERTY(bool coreEnabled READ coreEnabled NOTIFY changed)
    Q_PROPERTY(QVariantList interfaces READ interfaces NOTIFY changed)
    Q_PROPERTY(QVariantList routes READ routes NOTIFY changed)
    Q_PROPERTY(QVariantList vpnInterfaces READ vpnInterfaces NOTIFY changed)
    Q_PROPERTY(QVariantList diagnostics READ diagnostics NOTIFY changed)
    Q_PROPERTY(QStringList logs READ logs NOTIFY changed)
    Q_PROPERTY(int routeCount READ routeCount NOTIFY changed)
    Q_PROPERTY(int vpnCount READ vpnCount NOTIFY changed)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY changed)
public:
    explicit DashboardController(QObject* parent = nullptr);
    [[nodiscard]] QString coreStatus() const { return core_status_; }
    [[nodiscard]] QString profile() const { return profile_; }
    [[nodiscard]] QString vpnStatus() const { return vpn_status_; }
    [[nodiscard]] QString defaultRoute() const { return default_route_; }
    [[nodiscard]] QString lastUpdate() const { return last_update_; }
    [[nodiscard]] QString lastError() const { return last_error_; }
    [[nodiscard]] bool coreEnabled() const { return config_.engine_enabled; }
    [[nodiscard]] QVariantList interfaces() const { return interfaces_; }
    [[nodiscard]] QVariantList routes() const { return routes_; }
    [[nodiscard]] QVariantList vpnInterfaces() const { return vpn_interfaces_; }
    [[nodiscard]] QVariantList diagnostics() const { return diagnostics_; }
    [[nodiscard]] QStringList logs() const { return logs_; }
    [[nodiscard]] int routeCount() const { return routes_.size(); }
    [[nodiscard]] int vpnCount() const { return vpn_interfaces_.size(); }
    [[nodiscard]] int warningCount() const { return warning_count_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setProfile(const QString& profile);
    Q_INVOKABLE void setCoreEnabled(bool enabled);
signals:
    void changed();
private:
    static std::filesystem::path configPath();
    [[nodiscard]] QJsonObject requestService(const QString& command, const QJsonObject& payload = {}) const;
    void applyServiceSnapshot(const QJsonObject& snapshot);
    void appendLog(const QString& line);
    scpdpi::PlatformProviders providers_;
    scpdpi::CoreEngine engine_;
    scpdpi::ConfigStore config_store_;
    scpdpi::AppConfig config_;
    std::filesystem::path config_path_;
    QTimer* timer_{};
    QString core_status_{QStringLiteral("Starting")};
    QString profile_{QStringLiteral("default")};
    QString vpn_status_{QStringLiteral("Not detected")};
    QString default_route_{QStringLiteral("Not available")};
    QString last_update_{QStringLiteral("Waiting for first scan")};
    QString last_error_;
    QVariantList interfaces_;
    QVariantList routes_;
    QVariantList vpn_interfaces_;
    QVariantList diagnostics_;
    QStringList logs_;
    int warning_count_{};
};
