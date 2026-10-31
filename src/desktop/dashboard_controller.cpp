#include "dashboard_controller.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <algorithm>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
QString family_name(scpdpi::AddressFamily family) { return family == scpdpi::AddressFamily::IPv4 ? QStringLiteral("IPv4") : QStringLiteral("IPv6"); }
QString severity_name(scpdpi::Diagnostic::Severity severity) {
    if (severity == scpdpi::Diagnostic::Severity::Error) return QStringLiteral("error");
    if (severity == scpdpi::Diagnostic::Severity::Warning) return QStringLiteral("warning");
    return QStringLiteral("info");
}
}

DashboardController::DashboardController(QObject* parent)
    : QObject(parent), providers_(scpdpi::make_platform_providers()),
      engine_(providers_.interfaces, providers_.routes), config_path_(configPath()) {
    const auto loaded = config_store_.load(config_path_);
    if (loaded) config_ = loaded.value();
    profile_ = QString::fromStdString(config_.profile);
    timer_ = new QTimer(this);
    timer_->setInterval(static_cast<int>(config_.scan_interval_ms));
    connect(timer_, &QTimer::timeout, this, &DashboardController::refresh);
    timer_->start();
    appendLog(QStringLiteral("ScpDPI desktop initialized; network settings are read-only in 0.1.0."));
    refresh();
}

std::filesystem::path DashboardController::configPath() {
#ifdef _WIN32
    std::array<wchar_t, 32768> buffer{};
    const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0U && size < buffer.size()) return std::filesystem::path(buffer.data()) / L"ScpDPI" / L"config.json";
    return std::filesystem::temp_directory_path() / L"ScpDPI" / L"config.json";
#else
    if (const char* base = std::getenv("XDG_CONFIG_HOME"); base != nullptr && *base != '\0')
        return std::filesystem::path(base) / "scpdpi" / "config.json";
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".config" / "scpdpi" / "config.json";
    return std::filesystem::path("/tmp") / ("scpdpi-" + std::to_string(static_cast<unsigned long long>(getuid()))) / "config.json";
#endif
}

QJsonObject DashboardController::requestService(const QString& command, const QJsonObject& payload) const {
    QLocalSocket socket;
#ifdef _WIN32
    const QString endpoint = QStringLiteral("ScpDPI.v1");
#else
    QString endpoint;
    if (const char* runtime = std::getenv("XDG_RUNTIME_DIR"); runtime != nullptr && *runtime != '\0')
        endpoint = QString::fromLocal8Bit(runtime) + QStringLiteral("/scpdpi.sock");
    else
        endpoint = QStringLiteral("/tmp/scpdpi-%1.sock").arg(static_cast<qulonglong>(getuid()));
#endif
    socket.connectToServer(endpoint, QIODevice::ReadWrite);
    if (!socket.waitForConnected(120)) return {};
    const QJsonDocument request(QJsonObject{{QStringLiteral("command"), command}, {QStringLiteral("payload"), payload}});
    QByteArray wire = request.toJson(QJsonDocument::Compact);
    wire.append('\n');
    if (socket.write(wire) != wire.size() || !socket.waitForBytesWritten(500)) return {};
    QByteArray response;
    constexpr qsizetype max_response_size = 4 * 1024 * 1024;
    while (!response.contains('\n') && response.size() <= max_response_size) {
        if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(2500)) return {};
        response.append(socket.readAll());
    }
    const auto newline = response.indexOf('\n');
    if (newline < 0) return {};
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(response.first(newline), &error);
    return error.error == QJsonParseError::NoError && doc.isObject() ? doc.object() : QJsonObject{};
}

void DashboardController::applyServiceSnapshot(const QJsonObject& snapshot) {
    interfaces_.clear(); routes_.clear(); vpn_interfaces_.clear(); diagnostics_.clear();
    for (const auto& item : snapshot.value(QStringLiteral("interfaces")).toArray()) {
        const auto iface = item.toObject();
        QVariantList addresses;
        for (const auto& address_value : iface.value(QStringLiteral("addresses")).toArray()) {
            const auto address = address_value.toObject();
            addresses.push_back(QVariantMap{{QStringLiteral("address"), address.value(QStringLiteral("address")).toString()},
                                            {QStringLiteral("family"), QString::fromStdString(address.value(QStringLiteral("family")).toString().toStdString())}});
        }
        const auto mtu = iface.value(QStringLiteral("mtu")).toInt();
        interfaces_.push_back(QVariantMap{{QStringLiteral("index"), iface.value(QStringLiteral("index")).toInt()},
            {QStringLiteral("name"), iface.value(QStringLiteral("name")).toString()},
            {QStringLiteral("description"), iface.value(QStringLiteral("description")).toString()},
            {QStringLiteral("mac"), iface.value(QStringLiteral("mac")).toString().isEmpty() ? QStringLiteral("Unavailable") : iface.value(QStringLiteral("mac")).toString()},
            {QStringLiteral("mtu"), mtu == 0 ? QStringLiteral("Unavailable") : QString::number(mtu)},
            {QStringLiteral("kind"), iface.value(QStringLiteral("kind")).toString()},
            {QStringLiteral("up"), iface.value(QStringLiteral("up")).toBool()},
            {QStringLiteral("virtual"), iface.value(QStringLiteral("virtual")).toBool()},
            {QStringLiteral("addresses"), addresses}});
    }
    for (const auto& item : snapshot.value(QStringLiteral("routes")).toArray()) {
        const auto route = item.toObject();
        routes_.push_back(QVariantMap{{QStringLiteral("destination"), route.value(QStringLiteral("destination")).toString() + QStringLiteral("/") + QString::number(route.value(QStringLiteral("prefix")).toInt())},
            {QStringLiteral("family"), route.value(QStringLiteral("destination")).toString().contains(QLatin1Char(':')) ? QStringLiteral("IPv6") : QStringLiteral("IPv4")},
            {QStringLiteral("gateway"), route.value(QStringLiteral("gateway")).toString().isEmpty() ? QStringLiteral("On-link") : route.value(QStringLiteral("gateway")).toString()},
            {QStringLiteral("interface"), route.value(QStringLiteral("interface")).toString()},
            {QStringLiteral("metric"), route.value(QStringLiteral("metric")).toInt()},
            {QStringLiteral("default"), route.value(QStringLiteral("default")).toBool()}});
    }
    for (const auto& item : snapshot.value(QStringLiteral("vpn_interfaces")).toArray()) {
        const auto vpn = item.toObject();
        QStringList evidence;
        for (const auto& entry : vpn.value(QStringLiteral("evidence")).toArray()) evidence.push_back(entry.toString());
        const auto confidence = vpn.value(QStringLiteral("confidence")).toString();
        const QString pretty = confidence == QStringLiteral("confirmed") ? QStringLiteral("Confirmed") :
                               confidence == QStringLiteral("probable") ? QStringLiteral("Probable") : QStringLiteral("Unknown");
        vpn_interfaces_.push_back(QVariantMap{{QStringLiteral("name"), vpn.value(QStringLiteral("name")).toString()},
            {QStringLiteral("confidence"), pretty}, {QStringLiteral("evidence"), evidence.join(QStringLiteral(" · "))}});
    }
    for (const auto& item : snapshot.value(QStringLiteral("diagnostics")).toArray()) {
        const auto finding = item.toObject();
        diagnostics_.push_back(QVariantMap{{QStringLiteral("code"), finding.value(QStringLiteral("code")).toString()},
            {QStringLiteral("severity"), finding.value(QStringLiteral("severity")).toString()},
            {QStringLiteral("message"), finding.value(QStringLiteral("message")).toString()}});
    }
    warning_count_ = snapshot.value(QStringLiteral("telemetry")).toObject().value(QStringLiteral("warning_count")).toInt();
    const auto vpns = snapshot.value(QStringLiteral("vpn_interfaces")).toArray();
    if (vpns.isEmpty()) vpn_status_ = QStringLiteral("No VPN interface detected");
    else {
        const auto primary = vpns.first().toObject();
        const auto confidence = primary.value(QStringLiteral("confidence")).toString();
        vpn_status_ = primary.value(QStringLiteral("name")).toString() + QStringLiteral(" · ") +
            (confidence == QStringLiteral("confirmed") ? QStringLiteral("confirmed tunnel evidence") :
             confidence == QStringLiteral("probable") ? QStringLiteral("probable tunnel evidence") : QStringLiteral("unclassified virtual interface"));
    }
    const QJsonObject telemetry = snapshot.value(QStringLiteral("telemetry")).toObject();
    const qint64 updated = static_cast<qint64>(telemetry.value(QStringLiteral("updated_at_unix_ms")).toDouble());
    last_update_ = updated > 0 ? QDateTime::fromMSecsSinceEpoch(updated).toLocalTime().toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("Unavailable");
    const auto route_array = snapshot.value(QStringLiteral("routes")).toArray();
    QJsonObject best_route;
    for (const auto& entry : route_array) {
        const auto route = entry.toObject();
        if (!route.value(QStringLiteral("default")).toBool()) continue;
        if (best_route.isEmpty() || route.value(QStringLiteral("metric")).toInt() < best_route.value(QStringLiteral("metric")).toInt()) best_route = route;
    }
    if (best_route.isEmpty()) default_route_ = QStringLiteral("No default route detected");
    else {
        const auto gateway = best_route.value(QStringLiteral("gateway")).toString();
        const QString family = best_route.value(QStringLiteral("destination")).toString().contains(QLatin1Char(':')) ? QStringLiteral("IPv6") : QStringLiteral("IPv4");
        default_route_ = family + QStringLiteral(" via ") + (gateway.isEmpty() ? QStringLiteral("on-link") : gateway) +
            QStringLiteral(" · ") + best_route.value(QStringLiteral("interface")).toString();
    }
}

void DashboardController::appendLog(const QString& line) {
    logs_.prepend(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + line);
    while (logs_.size() > 200) logs_.removeLast();
}

void DashboardController::refresh() {
    const auto service_status = requestService(QStringLiteral("status"));
    const bool service_connected = service_status.value(QStringLiteral("ok")).toBool();
    if (service_connected) {
        const bool running = service_status.value(QStringLiteral("running")).toBool();
        const QString active_profile = service_status.value(QStringLiteral("profile")).toString(QStringLiteral("default"));
        config_.engine_enabled = running;
        profile_ = active_profile;
        core_status_ = running ? QStringLiteral("Running") : QStringLiteral("Stopped");
        QJsonObject snapshot = service_status.value(QStringLiteral("snapshot")).toObject();
        if (running) {
            const auto scanned = requestService(QStringLiteral("scan"));
            if (scanned.value(QStringLiteral("ok")).toBool()) snapshot = scanned.value(QStringLiteral("snapshot")).toObject();
        }
        if (!snapshot.isEmpty()) applyServiceSnapshot(snapshot);
        last_error_.clear();
        appendLog(running ? QStringLiteral("Network scan completed through the local service.") : QStringLiteral("Core service is stopped."));
        emit changed();
        return;
    }
    if (!config_.engine_enabled) {
        core_status_ = QStringLiteral("Stopped");
        emit changed();
        return;
    }
    auto result = engine_.scan();
    if (!result) {
        core_status_ = QStringLiteral("Error");
        last_error_ = QString::fromStdString(result.error().message);
        appendLog(QStringLiteral("Network scan failed: ") + last_error_);
        emit changed();
        return;
    }
    const auto& snapshot = result.value();
    interfaces_.clear(); routes_.clear(); vpn_interfaces_.clear(); diagnostics_.clear();
    for (const auto& iface : snapshot.interfaces) {
        QVariantList addresses;
        for (const auto& address : iface.addresses)
            addresses.push_back(QVariantMap{{QStringLiteral("address"), QString::fromStdString(address.to_string())},
                                            {QStringLiteral("family"), family_name(address.family)}});
        interfaces_.push_back(QVariantMap{{QStringLiteral("index"), iface.index},
            {QStringLiteral("name"), QString::fromStdString(iface.name)},
            {QStringLiteral("description"), QString::fromStdString(iface.description)},
            {QStringLiteral("mac"), iface.mac_address.empty() ? QStringLiteral("Unavailable") : QString::fromStdString(iface.mac_address)},
            {QStringLiteral("mtu"), iface.mtu == 0U ? QStringLiteral("Unavailable") : QString::number(iface.mtu)},
            {QStringLiteral("kind"), QString::fromStdString(iface.kind)}, {QStringLiteral("up"), iface.is_up},
            {QStringLiteral("virtual"), iface.is_virtual}, {QStringLiteral("addresses"), addresses}});
    }
    for (const auto& route : snapshot.routes) {
        routes_.push_back(QVariantMap{{QStringLiteral("destination"), QString::fromStdString(route.destination.to_string()) + "/" + QString::number(route.prefix_length)},
            {QStringLiteral("family"), family_name(route.destination.family)},
            {QStringLiteral("gateway"), route.gateway ? QString::fromStdString(route.gateway->to_string()) : QStringLiteral("On-link")},
            {QStringLiteral("interface"), QString::fromStdString(route.interface_name)},
            {QStringLiteral("metric"), route.metric}, {QStringLiteral("default"), route.is_default}});
    }
    const scpdpi::VpnInterface* primary_vpn = nullptr;
    for (const auto& vpn : snapshot.vpn_interfaces) {
        const QString confidence = vpn.confidence == scpdpi::VpnInterface::Confidence::Confirmed ? QStringLiteral("Confirmed") :
            vpn.confidence == scpdpi::VpnInterface::Confidence::Probable ? QStringLiteral("Probable") : QStringLiteral("Unknown");
        QStringList evidence; for (const auto& item : vpn.evidence) evidence.push_back(QString::fromStdString(item));
        vpn_interfaces_.push_back(QVariantMap{{QStringLiteral("name"), QString::fromStdString(vpn.interface_name)},
            {QStringLiteral("confidence"), confidence}, {QStringLiteral("evidence"), evidence.join(QStringLiteral(" · "))}});
        const auto rank = [](scpdpi::VpnInterface::Confidence confidence) {
            if (confidence == scpdpi::VpnInterface::Confidence::Confirmed) return 2;
            if (confidence == scpdpi::VpnInterface::Confidence::Probable) return 1;
            return 0;
        };
        if (primary_vpn == nullptr || rank(vpn.confidence) > rank(primary_vpn->confidence)) primary_vpn = &vpn;
    }
    for (const auto& diagnostic : snapshot.diagnostics) {
        diagnostics_.push_back(QVariantMap{{QStringLiteral("code"), QString::fromStdString(diagnostic.code)},
            {QStringLiteral("severity"), severity_name(diagnostic.severity)}, {QStringLiteral("message"), QString::fromStdString(diagnostic.message)}});
    }
    warning_count_ = static_cast<int>(snapshot.telemetry.warning_count);
    core_status_ = config_.engine_enabled ? QStringLiteral("Running") : QStringLiteral("Stopped");
    last_error_.clear();
    if (primary_vpn == nullptr) vpn_status_ = QStringLiteral("No VPN interface detected");
    else if (primary_vpn->confidence == scpdpi::VpnInterface::Confidence::Unknown) vpn_status_ = QStringLiteral("Unclassified virtual interface");
    else vpn_status_ = QString::fromStdString(primary_vpn->interface_name) + QStringLiteral(" · ") +
        (primary_vpn->confidence == scpdpi::VpnInterface::Confidence::Confirmed ? QStringLiteral("confirmed tunnel evidence") : QStringLiteral("probable tunnel evidence"));
    const auto best = std::min_element(snapshot.routes.begin(), snapshot.routes.end(), [](const auto& a, const auto& b) {
        if (a.is_default != b.is_default) return a.is_default;
        return a.metric < b.metric;
    });
    if (best == snapshot.routes.end() || !best->is_default) default_route_ = QStringLiteral("No default route detected");
    else default_route_ = family_name(best->destination.family) + QStringLiteral(" via ") +
        (best->gateway ? QString::fromStdString(best->gateway->to_string()) : QStringLiteral("on-link")) +
        QStringLiteral(" · ") + QString::fromStdString(best->interface_name);
    last_update_ = QDateTime::fromMSecsSinceEpoch(snapshot.telemetry.updated_at_unix_ms).toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    appendLog(QStringLiteral("Network scan completed: %1 interfaces, %2 routes.").arg(interfaces_.size()).arg(routes_.size()));
    emit changed();
}

void DashboardController::setProfile(const QString& value) {
    const auto name = value.toStdString();
    if (name != "default" && name != "vpn-safe" && name != "diagnostic" && name != "custom") return;
    const auto service_response = requestService(QStringLiteral("set_profile"), QJsonObject{{QStringLiteral("profile"), value}});
    if (service_response.value(QStringLiteral("ok")).toBool()) {
        config_.profile = name;
        profile_ = value;
        last_error_.clear();
        appendLog(QStringLiteral("Profile changed through the local service to ") + value + QStringLiteral("."));
        emit changed();
        return;
    }
    config_.profile = name;
    const auto result = config_store_.save(config_, config_path_);
    if (!result) {
        last_error_ = QString::fromStdString(result.error().message);
        appendLog(QStringLiteral("Profile could not be saved: ") + last_error_);
    } else {
        profile_ = value;
        last_error_.clear();
        appendLog(QStringLiteral("Profile changed to ") + value + QStringLiteral("."));
    }
    emit changed();
}

void DashboardController::setCoreEnabled(bool enabled) {
    if (config_.engine_enabled == enabled) return;
    const auto service_response = requestService(enabled ? QStringLiteral("start") : QStringLiteral("stop"));
    if (service_response.value(QStringLiteral("ok")).toBool()) {
        config_.engine_enabled = enabled;
        if (enabled) timer_->start(); else timer_->stop();
        core_status_ = enabled ? QStringLiteral("Running") : QStringLiteral("Stopped");
        last_error_.clear();
        appendLog(enabled ? QStringLiteral("Core scanning started through the local service.") : QStringLiteral("Core scanning stopped through the local service."));
        if (enabled) refresh();
        emit changed();
        return;
    }
    config_.engine_enabled = enabled;
    const auto saved = config_store_.save(config_, config_path_);
    if (!saved) {
        config_.engine_enabled = !enabled;
        last_error_ = QString::fromStdString(saved.error().message);
        appendLog(QStringLiteral("Core state could not be saved: ") + last_error_);
    } else if (enabled) {
        timer_->start();
        last_error_.clear();
        appendLog(QStringLiteral("Core scanning started."));
        refresh();
    } else {
        timer_->stop();
        core_status_ = QStringLiteral("Stopped");
        last_error_.clear();
        appendLog(QStringLiteral("Core scanning stopped."));
    }
    emit changed();
}
