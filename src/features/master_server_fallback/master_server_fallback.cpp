#include "feature_config.h"

#if FEATURE_MASTER_SERVER_FALLBACK

#include "generated_detours.h"
#include "sof_compat.h"
#include "util.h"

#if 0
// Legacy automatic probing and master rotation are intentionally disabled.
namespace {

constexpr char kMasterServersCvar[] = "_sofbuddy_master_gamespy_servers";
constexpr char kSoFPlusMasterCvar[] = "_sp_cl_master_gamespy";
constexpr char kDefaultMaster[] = "sof1master.sofvault.org";
constexpr char kDefaultMasterServers[] =
    "sof1master.megalag.org,master.333networks.com,sof1master.sofvault.org,gsm.qtracker.com,master2.qtracker.com";
constexpr char kMasterProbePacket[] = "IDQD";
constexpr unsigned short kMasterGamespyPort = 28900;
constexpr DWORD kMasterProbeTimeoutMs = 3000;
constexpr size_t kNoMaster = static_cast<size_t>(-1);

cvar_t* g_master_servers = nullptr;

enum class ProbeResult : int { None, Success, Failure };
enum class ProbePurpose { Startup, Refresh };

struct MasterRuntimeState {
    std::atomic<unsigned long> next_job{1};
    std::atomic<unsigned long> active_job{0};
    std::atomic<bool> worker_running{false};
    std::atomic<int> worker_result{static_cast<int>(ProbeResult::None)};
    std::atomic<size_t> worker_index{kNoMaster};
    size_t active_index = kNoMaster;
    size_t refresh_query_index = kNoMaster;
    size_t refresh_next_index = kNoMaster;
    size_t refresh_attempts = 0;
    bool initialized = false;
    bool startup_scan_started = false;
    bool refresh_pending = false;
    ProbePurpose probe_purpose = ProbePurpose::Startup;
};

MasterRuntimeState g_state;

std::string trim(std::string value) {
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

std::vector<std::string> master_servers() {
    std::vector<std::string> result;
    if (!g_master_servers || !g_master_servers->string) return result;

    std::string value(g_master_servers->string);
    size_t start = 0;
    while (start <= value.size()) {
        const size_t comma = value.find(',', start);
        std::string server = trim(value.substr(start, comma == std::string::npos ? comma : comma - start));
        if (!server.empty() && server.size() <= 63 &&
            server.find_first_of(" \t") == std::string::npos &&
            server.find_first_of("\";\r\n") == std::string::npos) {
            result.push_back(server);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return result;
}

void start_startup_scan();

void master_servers_changed(cvar_t*) {
    g_state.startup_scan_started = false;
    g_state.active_index = kNoMaster;
    g_state.refresh_pending = false;
    g_state.refresh_attempts = 0;
    g_state.active_job.store(0, std::memory_order_release);
    g_state.worker_running.store(false, std::memory_order_release);
    g_state.worker_result.store(static_cast<int>(ProbeResult::None), std::memory_order_release);
    if (g_state.initialized) start_startup_scan();
}

bool set_active_master(const std::string& server) {
    if (!detour_Cvar_Set2::oCvar_Set2 ||
        !findCvar(const_cast<char*>(kSoFPlusMasterCvar))) {
        return false;
    }
    detour_Cvar_Set2::oCvar_Set2(
        const_cast<char*>(kSoFPlusMasterCvar),
        const_cast<char*>(server.c_str()), true);
    return true;
}

void update_master_available_cvar();

bool set_active_master(const std::vector<std::string>& servers, size_t index) {
    if (index >= servers.size() || !set_active_master(servers[index])) return false;
    g_state.active_index = index;
    update_master_available_cvar();
    PrintOut(PRINT_LOG, "master_server_fallback: using %s\n", servers[index].c_str());
    return true;
}

std::string gamespy_validate(const std::string& challenge) {
    static const char key[] = "iVn3a3";
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (challenge.empty() || challenge.size() > 65) return std::string();

    unsigned char table[256];
    unsigned char encoded[68];
    int a = 0;
    int b = 0;
    for (int i = 0; i < 256; ++i) table[i] = static_cast<unsigned char>(i);
    for (int i = 0; i < 256; ++i) {
        a = (a + table[i] + static_cast<unsigned char>(key[i % 6])) & 255;
        std::swap(table[i], table[a]);
    }
    a = 0;
    b = 0;
    for (size_t i = 0; i < challenge.size(); ++i) {
        a = (a + static_cast<unsigned char>(challenge[i]) + 1) & 255;
        const unsigned char x = table[a];
        b = (b + x) & 255;
        const unsigned char y = table[b];
        table[b] = x;
        table[a] = y;
        encoded[i] = static_cast<unsigned char>(
            challenge[i]) ^ table[(x + y) & 255];
    }

    const size_t padded = (challenge.size() + 2) / 3 * 3;
    for (size_t i = challenge.size(); i < padded; ++i) encoded[i] = 0;
    std::string result;
    for (size_t i = 0; i < padded; i += 3) {
        const unsigned int x = encoded[i];
        const unsigned int y = encoded[i + 1];
        const unsigned int z = encoded[i + 2];
        result += alphabet[x >> 2];
        result += alphabet[((x & 3) << 4) | (y >> 4)];
        result += alphabet[((y & 15) << 2) | (z >> 6)];
        result += alphabet[z & 63];
    }
    return result;
}

SOCKET connect_master_host(const std::string& host) {
    hostent* resolved = gethostbyname(host.c_str());
    if (!resolved || !resolved->h_addr_list || !resolved->h_addr_list[0]) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s DNS failed (%d)\n",
                 host.c_str(), WSAGetLastError());
        return INVALID_SOCKET;
    }

    sockaddr_in target;
    std::memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(kMasterGamespyPort);
    std::memcpy(&target.sin_addr, resolved->h_addr_list[0], sizeof(target.sin_addr));

    SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_handle == INVALID_SOCKET) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s socket failed (%d)\n",
                 host.c_str(), WSAGetLastError());
        return INVALID_SOCKET;
    }
    u_long nonblocking = 1;
    ioctlsocket(socket_handle, FIONBIO, &nonblocking);
    int status = connect(
        socket_handle, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    if (status == SOCKET_ERROR) {
        const int connect_error = WSAGetLastError();
        if (connect_error != WSAEWOULDBLOCK && connect_error != WSAEINPROGRESS &&
            connect_error != WSAEALREADY) {
            PrintOut(PRINT_DEV, "master_server_fallback: %s connect failed (%d)\n",
                     host.c_str(), connect_error);
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(socket_handle, &write_set);
        timeval timeout;
        timeout.tv_sec = static_cast<long>(kMasterProbeTimeoutMs / 1000);
        timeout.tv_usec = static_cast<long>((kMasterProbeTimeoutMs % 1000) * 1000);
        if (select(0, nullptr, &write_set, nullptr, &timeout) <= 0) {
            PrintOut(PRINT_DEV, "master_server_fallback: %s connect timed out (%d)\n",
                     host.c_str(), WSAGetLastError());
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }
        int socket_error = 0;
        int error_length = sizeof(socket_error);
        if (getsockopt(socket_handle, SOL_SOCKET, SO_ERROR,
                       reinterpret_cast<char*>(&socket_error),
                       &error_length) == SOCKET_ERROR ||
            socket_error != 0) {
            PrintOut(PRINT_DEV, "master_server_fallback: %s connect rejected (%d)\n",
                     host.c_str(), socket_error);
            closesocket(socket_handle);
            return INVALID_SOCKET;
        }
    }

    nonblocking = 0;
    ioctlsocket(socket_handle, FIONBIO, &nonblocking);
    DWORD timeout = kMasterProbeTimeoutMs;
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    return socket_handle;
}

bool send_master(SOCKET socket_handle, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const int remaining = static_cast<int>(data.size() - sent);
        const int count = send(socket_handle, data.data() + sent, remaining, 0);
        if (count <= 0) return false;
        sent += static_cast<size_t>(count);
    }
    return true;
}

bool probe_master_host(const std::string& host) {
    SOCKET socket_handle = connect_master_host(host);
    if (socket_handle == INVALID_SOCKET) return false;

    if (!send_master(socket_handle, kMasterProbePacket)) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s IDQD send failed (%d)\n",
                 host.c_str(), WSAGetLastError());
        closesocket(socket_handle);
        return false;
    }

    const std::string marker = "\\secure\\";
    std::string received;
    char challenge_data[128];
    std::string challenge;
    while (received.size() < 512) {
        const int challenge_size = recv(socket_handle, challenge_data,
                                        sizeof(challenge_data), 0);
        if (challenge_size <= 0) break;
        received.append(challenge_data, challenge_size);
        const size_t marker_start = received.find(marker);
        if (marker_start == std::string::npos) continue;
        const size_t challenge_start = marker_start + marker.size();
        if (received.size() < challenge_start + 6) continue;
        const size_t challenge_end = received.find('\\', challenge_start);
        challenge = received.substr(
            challenge_start, challenge_end == std::string::npos
                ? 6 : challenge_end - challenge_start);
        break;
    }
    if (challenge.empty()) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s challenge failed (%d)\n",
                 host.c_str(), WSAGetLastError());
        closesocket(socket_handle);
        return false;
    }
    const std::string validation = gamespy_validate(challenge);
    if (validation.empty()) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s validation failed\n",
                 host.c_str());
        closesocket(socket_handle);
        return false;
    }

    const std::string validation_packet =
        "\\gamename\\sofretail\\gamever\\1.07f\\location\\0\\validate\\" +
        validation + "\\final\\\\queryid\\1.1\\";
    if (!send_master(socket_handle, validation_packet) ||
        !send_master(socket_handle, "\\list\\cmp\\gamename\\sofretail\\final\\")) {
        PrintOut(PRINT_DEV, "master_server_fallback: %s list request failed (%d)\n",
                 host.c_str(), WSAGetLastError());
        closesocket(socket_handle);
        return false;
    }

    std::vector<char> list;
    char buffer[4096];
    static const char final_marker[] = "\\final\\";
    const size_t final_size = sizeof(final_marker) - 1;
    while (list.size() < 65536) {
        const int count = recv(socket_handle, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        list.insert(list.end(), buffer, buffer + count);
        const std::vector<char>::const_iterator final =
            std::search(list.begin(), list.end(),
                        final_marker, final_marker + final_size);
        if (final != list.end()) {
            list.resize(static_cast<size_t>(final - list.begin()) + final_size);
            break;
        }
    }
    closesocket(socket_handle);
    const bool valid = list.size() > final_size &&
        (list.size() - final_size) % 6 == 0 &&
        std::memcmp(list.data() + list.size() - final_size,
                    final_marker, final_size) == 0;
    if (!valid)
        PrintOut(PRINT_DEV, "master_server_fallback: %s invalid list (%u bytes, %d)\n",
                 host.c_str(), static_cast<unsigned>(list.size()), WSAGetLastError());
    return valid;
}

void publish_probe_result(unsigned long job, ProbeResult result, size_t index) {
    if (g_state.active_job.load(std::memory_order_acquire) != job) return;
    g_state.worker_index.store(index, std::memory_order_release);
    g_state.worker_result.store(static_cast<int>(result), std::memory_order_release);
    g_state.worker_running.store(false, std::memory_order_release);
}

void probe_worker(std::vector<std::string> servers, size_t start, size_t count,
                  unsigned long job) {
    for (size_t offset = 0; offset < count; ++offset) {
        if (g_state.active_job.load(std::memory_order_acquire) != job) return;
        const size_t index = (start + offset) % servers.size();
        if (probe_master_host(servers[index])) {
            publish_probe_result(job, ProbeResult::Success, index);
            return;
        }
    }
    publish_probe_result(job, ProbeResult::Failure, kNoMaster);
}

bool start_probe(const std::vector<std::string>& servers, size_t start, size_t count,
                 ProbePurpose purpose) {
    if (servers.empty() || count == 0 || g_state.worker_running.load(std::memory_order_acquire))
        return false;

    const unsigned long job = g_state.next_job.fetch_add(1, std::memory_order_relaxed);
    g_state.probe_purpose = purpose;
    g_state.active_job.store(job, std::memory_order_release);
    g_state.worker_index.store(kNoMaster, std::memory_order_release);
    g_state.worker_result.store(static_cast<int>(ProbeResult::None), std::memory_order_release);
    g_state.worker_running.store(true, std::memory_order_release);
    try {
        std::thread(probe_worker, servers, start, count, job).detach();
    } catch (...) {
        g_state.worker_index.store(kNoMaster, std::memory_order_release);
        g_state.worker_result.store(static_cast<int>(ProbeResult::Failure), std::memory_order_release);
        g_state.worker_running.store(false, std::memory_order_release);
        PrintOut(PRINT_BAD, "master_server_fallback: probe worker failed to start\n");
    }
    return true;
}

bool is_pingservers_command(const char* text) {
    if (!text) return false;
    while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
    const char* end = text;
    while (*end && !std::isspace(static_cast<unsigned char>(*end)) && *end != ';') ++end;
    if (end - text != 11) return false;
    for (size_t i = 0; i < 11; ++i)
        if (std::tolower(static_cast<unsigned char>(text[i])) != "pingservers"[i]) return false;
    return true;
}

void issue_pingservers() {
    if (detour_Cmd_ExecuteString::oCmd_ExecuteString)
        detour_Cmd_ExecuteString::oCmd_ExecuteString("pingservers");
}

void master_server_fallback_pump();

void begin_refresh() {
    if (!g_state.initialized || g_state.refresh_pending) return;
    if (!g_state.worker_running.load(std::memory_order_acquire) &&
        g_state.probe_purpose == ProbePurpose::Startup &&
        g_state.worker_result.load(std::memory_order_acquire) != static_cast<int>(ProbeResult::None)) {
        master_server_fallback_pump();
    }
    const std::vector<std::string> servers = master_servers();
    if (servers.empty()) {
        PrintOut(PRINT_BAD, "master_server_fallback: no valid master servers configured\n");
        return;
    }

    g_state.refresh_pending = true;
    g_state.refresh_query_index =
        g_state.active_index == kNoMaster ? 0 : g_state.active_index % servers.size();
    g_state.refresh_attempts = 1;
    g_state.refresh_next_index = (g_state.refresh_query_index + 1) % servers.size();

    if (g_state.active_index == kNoMaster &&
        !set_active_master(servers, g_state.refresh_query_index)) {
        g_state.refresh_pending = false;
        return;
    }

    if (g_state.worker_running.load(std::memory_order_acquire)) {
        // The startup scan already probes this list. Its result also resolves this refresh.
        return;
    }
    start_probe(servers, g_state.refresh_query_index, 1, ProbePurpose::Refresh);
}

void Cmd_SoFBuddy_Master_Next_f() {
    const std::vector<std::string> servers = master_servers();
    if (servers.empty()) {
        PrintOut(PRINT_BAD, "master_server_fallback: no valid master servers configured\n");
        return;
    }

    g_state.active_job.store(0, std::memory_order_release);
    g_state.worker_running.store(false, std::memory_order_release);
    g_state.worker_result.store(static_cast<int>(ProbeResult::None), std::memory_order_release);
    g_state.refresh_pending = false;
    const size_t index = g_state.active_index == kNoMaster
        ? 0 : (g_state.active_index + 1) % servers.size();
    if (!set_active_master(servers, index)) {
        PrintOut(PRINT_BAD, "master_server_fallback: SoFPlus master cvar is unavailable\n");
        return;
    }
    g_state.refresh_pending = true;
    g_state.refresh_query_index = index;
    g_state.refresh_attempts = 1;
    g_state.refresh_next_index = (index + 1) % servers.size();
    start_probe(servers, index, 1, ProbePurpose::Refresh);
    issue_pingservers();
}

void update_master_available_cvar() {
    if (!detour_Cvar_Set2::oCvar_Set2) return;
    cvar_t* available = findCvar(const_cast<char*>("_sb_internal_master_gamespy_available"));
    if (available) {
        const char* value = findCvar(const_cast<char*>(kSoFPlusMasterCvar)) ? "1" : "0";
        detour_Cvar_Set2::oCvar_Set2(
            const_cast<char*>(available->name), const_cast<char*>(value), true);
    }
}

void start_startup_scan() {
    if (!g_state.initialized || g_state.startup_scan_started ||
        !findCvar(const_cast<char*>(kSoFPlusMasterCvar))) return;
    const std::vector<std::string> servers = master_servers();
    if (servers.empty()) {
        PrintOut(PRINT_BAD, "master_server_fallback: no valid master servers configured\n");
        g_state.startup_scan_started = true;
        return;
    }
    g_state.startup_scan_started = true;
    PrintOut(PRINT_LOG, "master_server_fallback: checking %zu master server(s)\n", servers.size());
    start_probe(servers, 0, servers.size(), ProbePurpose::Startup);
}

void master_server_fallback_pump() {
    const ProbeResult result = static_cast<ProbeResult>(
        g_state.worker_result.exchange(static_cast<int>(ProbeResult::None), std::memory_order_acq_rel));
    if (result == ProbeResult::None) return;

    const size_t result_index = g_state.worker_index.load(std::memory_order_acquire);
    const ProbePurpose purpose = g_state.probe_purpose;
    const std::vector<std::string> servers = master_servers();
    if (servers.empty()) {
        g_state.refresh_pending = false;
        return;
    }

    if (purpose == ProbePurpose::Startup) {
        if (result == ProbeResult::Success && result_index < servers.size()) {
            if (!set_active_master(servers, result_index)) {
                g_state.refresh_pending = false;
                return;
            }
            PrintOut(PRINT_LOG, "master_server_fallback: startup check selected %s\n",
                     servers[result_index].c_str());
            if (g_state.refresh_pending) {
                const bool retry = result_index != g_state.refresh_query_index;
                g_state.refresh_pending = false;
                if (retry) issue_pingservers();
            }
        } else {
            PrintOut(PRINT_BAD, "master_server_fallback: no responding master found at startup\n");
            g_state.refresh_pending = false;
        }
        return;
    }

    if (!g_state.refresh_pending) return;
    if (result == ProbeResult::Success && result_index < servers.size()) {
        const bool retry = result_index != g_state.refresh_query_index;
        if (!set_active_master(servers, result_index)) {
            g_state.refresh_pending = false;
            return;
        }
        g_state.refresh_pending = false;
        if (retry) issue_pingservers();
        return;
    }

    if (g_state.refresh_attempts >= servers.size()) {
        g_state.refresh_pending = false;
        PrintOut(PRINT_BAD, "master_server_fallback: all configured masters timed out\n");
        return;
    }

    const size_t next = g_state.refresh_next_index;
    ++g_state.refresh_attempts;
    g_state.refresh_next_index = (next + 1) % servers.size();
    start_probe(servers, next, 1, ProbePurpose::Refresh);
}

} // namespace

void master_server_fallback_cmd_execute(
    const char* text, detour_Cmd_ExecuteString::tCmd_ExecuteString original) {
    if (is_pingservers_command(text)) begin_refresh();
    if (original) original(text);
}

void master_server_fallback_qcommon_frame_post_callback(int msec) {
    (void)msec;
    master_server_fallback_pump();
}

#endif

namespace {
constexpr char kSoFPlusMasterCvar[] = "_sp_cl_master_gamespy";
constexpr char kDefaultMaster[] = "sof1master.sofvault.org";
}

void master_server_fallback_PreCvarInit() {
    if (!detour_Cvar_Get::oCvar_Get) return;
    if (findCvar(const_cast<char*>(kSoFPlusMasterCvar))) return;
    detour_Cvar_Get::oCvar_Get(
        kSoFPlusMasterCvar, kDefaultMaster, CVAR_ARCHIVE, nullptr);
}

void master_server_fallback_PostCvarInit() {
    if (!detour_Cvar_Get::oCvar_Get) return;

    cvar_t* master = findCvar(const_cast<char*>(kSoFPlusMasterCvar));
    if (!master)
        master = detour_Cvar_Get::oCvar_Get(
            kSoFPlusMasterCvar, kDefaultMaster, CVAR_ARCHIVE, nullptr);
    detour_Cvar_Get::oCvar_Get(
        "_sb_internal_feature_master_server_fallback", "1", 0, nullptr);
    if (master && (!master->string || !master->string[0]) &&
        detour_Cvar_Set2::oCvar_Set2)
        detour_Cvar_Set2::oCvar_Set2(
            const_cast<char*>(kSoFPlusMasterCvar),
            const_cast<char*>(kDefaultMaster), true);
}

#endif // FEATURE_MASTER_SERVER_FALLBACK
