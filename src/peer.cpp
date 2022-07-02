/* This file is part of the the dci project. Copyright (C) 2013-2022 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#include "pch.hpp"
#include "peer.hpp"

namespace dci::module::ppn::discovery
{
    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Peer::Peer()
        : idl::ppn::discovery::Peer<>::Opposite{idl::interface::Initializer()}
        , _nextIos4RegularGet{_ios.end()}
    {

        {
            node::link::Feature<>::Opposite op = *this;

            op->setup() += sol() * [this](node::link::feature::Service<> srv)
            {
                srv->addPayload(*this);

                srv->joinedByConnect() += sol() * [this](const node::link::Id& id, node::link::Remote<> r)
                {
                    joined(id, r);
                };

                srv->joinedByAccept() += sol() * [this](const node::link::Id& id, node::link::Remote<> r)
                {
                    joined(id, r);
                };

            };
        }

        {
            node::link::feature::Payload<>::Opposite op = *this;

            //in ids() -> set<ilid>;
            op->ids() += sol() * []()
            {
                return cmt::readyFuture(Set<idl::interface::Lid>{api::Supplier<>::lid()});
            };

            //in getInstance(Id requestorId, Remote requestor, ilid) -> interface;
            op->getInstance() += sol() * [this](const node::link::Id& id, const node::link::Remote<>& r, idl::interface::Lid ilid)
            {
                if(api::Supplier<>::lid() == ilid)
                {
                    peer::Io& io = _ios.emplace(std::piecewise_construct_t{},
                                                std::tie(r),
                                                std::forward_as_tuple(this, id)).first->second;

                    return cmt::readyFuture(idl::Interface{io.getSupplierOut()});
                }

                dbgWarn("crazy link?");

                return cmt::readyFuture<idl::Interface>(exception::buildInstance<api::Error>("bad instance ilid requested"));
            };
        }

        {
            node::Feature<>::Opposite op = *this;

            op->setup() += sol() * [this](node::feature::Service<> srv)
            {
                srv->start() += sol() * [this]() mutable
                {
                    _started = true;
                    _regularGetTicker.start();
                };

                srv->stop() += sol() * [this]
                {
                    _regularGetTicker.stop();
                    _started = false;
                    _ios.clear();
                    _nextIos4RegularGet = _ios.end();
                };

                srv->declared() += sol() * [this](const transport::Address& a)
                {
                    if(!_localAddresses.insert(a).second)
                    {
                        return;
                    }

                    if(_started)
                    {
                        for(auto&[r, io] : _ios)
                        {
                            if(utils::net::url::isCover(io.address().value, a.value))
                            {
                                io.declared(a);
                            }
                        }
                    }
                };

                srv->undeclared() += sol() * [this](const transport::Address& a)
                {
                    _localAddresses.erase(a);
                };

                _ras = srv;
            };
        }

        {
            idl::Configurable<>::Opposite op = *this;

            op->configure() += sol() * [this](dci::idl::Config&& config)
            {
                auto c = config::cnvt(std::move(config));

                setIntensity(std::atof(c.get("intensity", "1").data()));

                return cmt::readyFuture<>();
            };
        }
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    Peer::~Peer()
    {
        _ios.clear();
        _nextIos4RegularGet = _ios.end();
        sol().flush();
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Peer::joined(const node::link::Id& id, const node::link::Remote<>& r)
    {
        if(!_started) return;

        peer::Io& io = _ios.emplace(std::piecewise_construct_t{},
                                    std::tie(r),
                                    std::forward_as_tuple(this, id)).first->second;

        io.joined(r);
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Peer::free(const node::link::Remote<>& r)
    {
        Ios::iterator iter = _ios.find(r);
        if(_ios.end() == iter)
        {
            return;
        }

        if(_nextIos4RegularGet == iter)
        {
            _nextIos4RegularGet = _ios.erase(iter);
        }
        else
        {
            _ios.erase(iter);
        }
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Peer::input(const transport::Address& remoteAddr, const node::link::Id& remoteId, transport::Address&& remoteAddr2)
    {
        if(!utils::net::url::isCover(remoteAddr.value, remoteAddr2.value))
        {
            return;
        }

        _ras->fireDiscovered(remoteId, std::move(remoteAddr2));
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    List<transport::Address> Peer::output(const transport::Address& remoteAddr)
    {
        List<transport::Address> res;

        for(const transport::Address& a2 : _localAddresses)
        {
            if(utils::net::url::isCover(remoteAddr.value, a2.value))
            {
                res.emplace_back(a2);
            }
        }

        return res;
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Peer::regularGet()
    {
        if(_ios.empty())
        {
            return;
        }

        if(_nextIos4RegularGet == _ios.end())
        {
            _nextIos4RegularGet = _ios.begin();
        }
        else
        {
            ++_nextIos4RegularGet;
            if(_nextIos4RegularGet == _ios.end())
            {
                _nextIos4RegularGet = _ios.begin();
            }
        }

        _nextIos4RegularGet->second.regularGet();
    }

    /////////0/////////1/////////2/////////3/////////4/////////5/////////6/////////7
    void Peer::setIntensity(double v)
    {
        static constexpr int64 min = 50;
        static constexpr int64 max = int64{1000}*60*60*24;

        int64 ms;
        if(v <= 0)  ms = max;
        else        ms = std::clamp(static_cast<int64>(1000.0/v), min, max);

        _regularGetTicker.interval(std::chrono::milliseconds{ms});
    }
}
