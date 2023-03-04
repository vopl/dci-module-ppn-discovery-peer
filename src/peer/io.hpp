/* This file is part of the the dci project. Copyright (C) 2013-2023 vopl, shtoba.
   This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public
   License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
   This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
   You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>. */

#pragma once

#include "pch.hpp"

namespace dci::module::ppn::discovery
{
    class Peer;
}

namespace dci::module::ppn::discovery::peer
{
    class Io
    {
    public:
        Io(Peer* srv, const node::link::Id& id);
        ~Io();

        const transport::Address& address() const;

        void joined(const node::link::Remote<>& r);
        api::Supplier<> getSupplierOut();
        void declared(const transport::Address& a);

        void regularGet();

    private:
        Peer *                      _srv {};
        node::link::Id              _id {};

        transport::Address          _address;
        api::Supplier<>             _supplierIn;
        api::Supplier<>::Opposite   _supplierOut;

        sbs::Owner _sol;
    };
}
