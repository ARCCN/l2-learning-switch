#include "L2LearningSwitch.hpp"

#include "PacketParser.hpp"
#include "api/Packet.hpp"
#include <runos/core/logging.hpp>

#include <sstream>

namespace runos {

REGISTER_APPLICATION(L2LearningSwitch, {"controller",
                                "switch-manager",
                                "topology",
                                ""})

void L2LearningSwitch::init(Loader* loader, const Config& config)
{
    switch_manager_ = SwitchManager::get(loader);
    connect(switch_manager_, &SwitchManager::switchUp,
            this, &L2LearningSwitch::onSwitchUp);

    auto data_base = std::make_shared<HostsDatabase>();

    handler_ = Controller::get(loader)->register_handler(
    [=](of13::PacketIn& pi, OFConnectionPtr ofconn) mutable -> bool
    {
        PacketParser pp(pi);
        runos::Packet& pkt(pp);

        src_mac_ = pkt.load(ofb::eth_src);
        dst_mac_ = pkt.load(ofb::eth_dst);
        in_port_ = pkt.load(ofb::in_port);
        dpid_ = ofconn->dpid();
        
        if (not data_base->setPort(dpid_,
                                    src_mac_,
                                    in_port_)) {
            return false;
        }
        
        auto target_port = data_base->getPort(dpid_, dst_mac_);
        if (target_port != boost::none) {
            send_unicast(*target_port, pi);

        } else {
            send_broadcast(pi);
        }

        return true;
    }, -5);
}

void L2LearningSwitch::onSwitchUp(SwitchPtr sw)
{
    of13::FlowMod fm;
    fm.command(of13::OFPFC_ADD);
    fm.table_id(0);
    fm.priority(1);
    of13::ApplyActions applyActions;
    of13::OutputAction output_action(of13::OFPP_CONTROLLER, 0xFFFF);
    applyActions.add_action(output_action);
    fm.add_instruction(applyActions);
    sw->connection()->send(fm);
}

void L2LearningSwitch::send_unicast(uint32_t target_port,
                            const of13::PacketIn& pi)
{
    { // Send PacketOut.

    of13::PacketOut po;
    po.data(pi.data(), pi.data_len());
    of13::OutputAction output_action(target_port, of13::OFPCML_NO_BUFFER);
    po.add_action(output_action);
    switch_manager_->switch_(dpid_)->connection()->send(po);

    } // Send PacketOut.

    { // Create FlowMod.

    of13::FlowMod fm;
    fm.command(of13::OFPFC_ADD);
    fm.table_id(0);
    fm.priority(2);
    std::stringstream ss;
    fm.idle_timeout(uint64_t(60));
    fm.hard_timeout(uint64_t(1800));

    ss.str(std::string());
    ss.clear();
    ss << src_mac_;
    fm.add_oxm_field(new of13::EthSrc{
            fluid_msg::EthAddress(ss.str())});
    ss.str(std::string());
    ss.clear();
    ss << dst_mac_;
    fm.add_oxm_field(new of13::EthDst{
            fluid_msg::EthAddress(ss.str())});

    of13::ApplyActions applyActions;
    of13::OutputAction output_action(target_port, of13::OFPCML_NO_BUFFER);
    applyActions.add_action(output_action);
    fm.add_instruction(applyActions);
    switch_manager_->switch_(dpid_)->connection()->send(fm);

    } // Create FlowMod.
}

void L2LearningSwitch::send_broadcast(const of13::PacketIn& pi)
{
    of13::PacketOut po;
    po.data(pi.data(), pi.data_len());
    po.in_port(in_port_);
    of13::OutputAction output_action(of13::OFPP_ALL, of13::OFPCML_NO_BUFFER);
    po.add_action(output_action);
    switch_manager_->switch_(dpid_)->connection()->send(po);
}

bool HostsDatabase::setPort(uint64_t dpid,
                            ethaddr mac,
                            uint32_t in_port)
{
    if (is_broadcast(mac)) {
        LOG(WARNING) << "Broadcast source address, dropping";
        return false;
    }

    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    seen_ports_[dpid][mac] = in_port;
    return true;
}

boost::optional<uint32_t> HostsDatabase::getPort(uint64_t dpid, ethaddr mac)
{
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    auto it = seen_ports_[dpid].find(mac);
    
    if (it != seen_ports_[dpid].end()) {
        return it->second;
    
    } else {
        return boost::none;
    }
}

} // namespace runos
