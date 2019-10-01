#pragma once

#include "Application.hpp"
#include "Loader.hpp"
#include "SwitchManager.hpp"
#include "api/SwitchFwd.hpp"
#include "oxm/openflow_basic.hh"

#include <boost/optional.hpp>
#include <boost/thread.hpp>

#include <unordered_map>

namespace runos {

using SwitchPtr = safe::shared_ptr<Switch>;
namespace of13 = fluid_msg::of13;

namespace ofb {
    constexpr auto in_port = oxm::in_port();
    constexpr auto eth_src = oxm::eth_src();
    constexpr auto eth_dst = oxm::eth_dst();
}

class L2LearningSwitch : public Application
{
    Q_OBJECT
    SIMPLE_APPLICATION(L2LearningSwitch, "l2-learning-switch")
public:
    void init(Loader* loader, const Config& config) override;

protected slots:
    void onSwitchUp(SwitchPtr sw);

private:
    OFMessageHandlerPtr handler_;
    SwitchManager* switch_manager_;
	
    ethaddr src_mac_;
    ethaddr dst_mac_;
    uint64_t dpid_;
    uint32_t in_port_;

    void send_unicast(uint32_t target_switch_and_port, const of13::PacketIn& pi);
    void send_broadcast(const of13::PacketIn& pi);
};

class HostsDatabase
{
public:
    bool setPort(uint64_t dpid, ethaddr mac, uint32_t in_port);
    boost::optional<uint32_t> getPort(uint64_t dpid, ethaddr mac);

private:
    boost::shared_mutex mutex_;
    std::unordered_map<uint64_t,
            std::unordered_map<ethaddr, uint32_t>> seen_ports_;
};

} // namespace runos
