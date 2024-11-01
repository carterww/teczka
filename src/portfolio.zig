const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const _equity = @import("./equity.zig");
const Equity = _equity.Equity;
const EquityOwnership = _equity.EquityOwnership;
const EquityOwnershipOut = _equity.EquityOwnershipOut;
const EquityOwnershipStorage = _equity.EquityOwnershipStorage;

pub const PortfolioError = error{
    EquityNotFound,
};

pub const Portfolio = struct {
    // TODO: Figure out how we store equities
    market_value_cents: i64,
    cost_basis_cents: i64,
    delta_lifetime_absolute_cents: i64,
    delta_lifetime_basis_points: i64,
    delta_daily_absolute_cents: i64,
    delta_daily_basis_points: i64,

    pub fn init(self: *Portfolio, allocator: Allocator, estimated_capacity: ?usize) !void {
        if (estimated_capacity) |cap| {
            try self.equities.initCapacity(allocator, cap);
        } else {
            try self.equities.init(allocator);
        }
        self.market_value_cents = 0;
        self.cost_basis_cents = 0;
        self.delta_lifetime_absolute_cents = 0;
        self.delta_lifetime_basis_points = 0;
        self.delta_daily_absolute_cents = 0;
        self.delta_daily_basis_points = 0;
    }

    // Add equity ownership information to the portfolio. If the equity
    // exists in the portfolio, we add the values to the existing lot.
    // If it does not, we add it to the list.
    pub fn equity_add(
        self: *Portfolio,
        key: []const u8,
        equity_ownership: *const EquityOwnership,
    ) !void {
        assert(key.len > 0);
        _ = self;
        _ = equity_ownership;
    }
};

fn equity_ownership_lot_add(
    port: *const Portfolio,
    ownership_curr: *EquityOwnershipStorage,
    lot: *const EquityOwnership,
) !void {
    _ = port;
    ownership_curr.share_count_hundredths += lot.share_count_hundredths;
    ownership_curr.cost_basis_cents += lot.cost_basis_cents;
    try equity_ownership_deltas_update(ownership_curr);
}

fn equity_ownership_deltas_update(
    port: *const Portfolio,
    ownership_curr: *EquityOwnershipStorage,
) !void {
    _ = port;
    const eq = ownership_curr.equity orelse return PortfolioError.EquityNotFound;
    ownership_curr.delta_lifetime_absolute_cents =
        _equity.equity_total_value_cents(
        eq.price_cents_current,
        ownership_curr.share_count_hundredths,
    ) - ownership_curr.cost_basis_cents;
    ownership_curr.delta_lifetime_basis_points =
        _equity.delta_basis_points(
        ownership_curr.delta_lifetime_absolute_cents,
        ownership_curr.cost_basis_cents,
    );
    ownership_curr.delta_daily_absolute_cents =
        _equity.equity_total_value_cents(
        eq.price_cents_current - eq.price_cents_close_previous,
        ownership_curr.share_count_hundredths,
    );
    ownership_curr.delta_daily_basis_points =
        _equity.delta_basis_points(
        ownership_curr.delta_daily_absolute_cents,
        ownership_curr.cost_basis_cents,
    );
}
