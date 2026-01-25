document.addEventListener("DOMContentLoaded", () => {
    loadDeviceInfo();
    loadNetworkStatus();
});

async function loadDeviceInfo() {
    const res = await fetch("/api/device/info");
    if (!res.ok) return;

    const data = await res.json();

    document.getElementById("device_name").value = data.device_name || "";
    document.getElementById("device_id").textContent = data.id || "-";
    document.getElementById("device_type").textContent = data.device_type || "-";
    document.getElementById("firmware_version").textContent = data.firmware_version || "-";
}

async function loadNetworkStatus() {
    const res = await fetch("/api/network/status");
    if (!res.ok) return;

    const data = await res.json();

    document.getElementById("mac_address").textContent =
        data.mac_address || "-";
    document.getElementById("ap_enabled").checked =
        !!data.ap?.enabled;
    document.getElementById("ap_ssid").value =
        data.ap?.ssid || "";
    document.getElementById("ap_ip").textContent =
        data.ap?.ip || "-";
    document.getElementById("sta_ssid").value =
        data.sta?.ssid || "";
    document.getElementById("sta_ip").textContent =
        data.sta?.ip || "-";
}

document.getElementById("device-form").addEventListener("submit", async (e) => {
    e.preventDefault();

    const deviceName = document.getElementById("device_name").value.trim();
    if (!deviceName) return;

    const res = await fetch("/api/device/info", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ device_name: deviceName })
    });

    if (res.ok) {
        await loadDeviceInfo();
    }
});

document.getElementById("network-form").addEventListener("submit", async (e) => {
    e.preventDefault();

    const payload = {
        ap_enabled: document.getElementById("ap_enabled").checked,
        ap_ssid: document.getElementById("ap_ssid").value.trim(),
        ap_password: document.getElementById("ap_password").value,
        sta_ssid: document.getElementById("sta_ssid").value.trim(),
        sta_password: document.getElementById("sta_password").value,
    };

    const res = await fetch("/api/network/ap/set", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
    });

    if (res.ok) {
        await loadNetworkStatus();
    }
});
