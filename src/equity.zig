const builtin = @import("std").builtin;
const Mutex = @import("std").Thread.Mutex;
const Allocator = @import("std").mem.Allocator;

pub const Equity = struct {
    name: ?[]const u8,
    key: []const u8,
    price_cents_current: i64,
    price_cents_open: i64,
    price_cents_close_previous: i64,
    daily_change_absolute_cents: i64,
    daily_change_basis_points: i64,

    pub fn valuation_update(self: *Equity, other: *const Equity) void {
        self.price_cents_current = other.price_cents_current;
        self.price_cents_open = other.price_cents_open;
        self.price_cents_close_previous = other.price_cents_close_previous;
        self.daily_change_absolute_cents = other.daily_change_absolute_cents;
        self.daily_change_basis_points = other.daily_change_basis_points;
    }
};

pub const EquityOwnership = struct {
    share_count_hundredths: i64,
    cost_basis_cents: i64,
    delta_lifetime_absolute_cents: i64,
    delta_lifetime_basis_points: i64,
    delta_daily_absolute_cents: i64,
    delta_daily_basis_points: i64,
};

pub const EquityOwnershipOut = struct {
    allocator: Allocator,
    name: ?[]const u8,
    key: []const u8,
    price_cents_current: i64,
    price_cents_open: i64,
    price_cents_close_previous: i64,
    daily_change_absolute_cents: i64,
    daily_change_basis_points: i64,
    share_count_hundredths: i64,
    cost_basis_cents: i64,
    delta_lifetime_absolute_cents: i64,
    delta_lifetime_basis_points: i64,
    delta_daily_absolute_cents: i64,
    delta_daily_basis_points: i64,

    pub fn init(
        self: *EquityOwnershipOut,
        allocator: Allocator,
        equity_ownership: *const EquityOwnershipStorage,
    ) !void {
        // Lock equity to prevent values changing from under us
        // while we copy
        equity_ownership.mutex.lock();
        defer equity_ownership.mutex.unlock();
        const equity = equity_ownership.*.equity;
        self.allocator = allocator;
        self.name = try allocator.dupe([]const u8, equity.*.name);
        self.key = try allocator.dupe([]const u8, equity.*.key);
        self.price_cents_current = equity.*.price_cents_current;
        self.price_cents_open = equity.*.price_cents_open;
        self.price_cents_close_previous = equity.*.price_cents_close_previous;
        self.daily_change_absolute_cents = equity.*.daily_change_absolute_cents;
        self.daily_change_basis_points = equity.*.daily_change_basis_points;
        self.share_count_hundredths = equity_ownership.*.share_count_hundredths;
        self.cost_basis_cents = equity_ownership.*.cost_basis_cents;
        self.delta_lifetime_absolute_cents = equity_ownership.*.delta_lifetime_absolute_cents;
        self.delta_lifetime_basis_points = equity_ownership.*.delta_lifetime_basis_points;
        self.delta_daily_absolute_cents = equity_ownership.*.delta_daily_absolute_cents;
        self.delta_daily_basis_points = equity_ownership.*.delta_daily_basis_points;
    }

    pub fn deinit(self: *EquityOwnershipOut) void {
        self.allocator.free(self.name);
        self.allocator.free(self.key);
    }
};

pub const EquityOwnershipStorage = struct {
    mutex: Mutex = .{},
    equity: ?*const Equity,
    share_count_hundredths: i64,
    cost_basis_cents: i64,
    delta_lifetime_absolute_cents: i64,
    delta_lifetime_basis_points: i64,
    delta_daily_absolute_cents: i64,
    delta_daily_basis_points: i64,
};

// Takes a price per share and share count (in cents and hundredths respectively)
// and returns the total value of the equity.
// Ex) Stock X price = $2.50 and I own 5 shares.
//     250 cents * 500 shares/100 = 1250 cents or 250 * 500 / 100 = 1250.
//     We mutliply first to keep precision
pub fn equity_total_value_cents(share_price_cents: i64, share_count_hundredths: i64) i64 {
    // If this overflows you are too rich to be using this crappy
    // software
    @setRuntimeSafety(false);
    return @divFloor(
        share_price_cents * share_count_hundredths,
        100,
    );
}

// Takes an absolute delta and the original value. It calculates the
// change in basis points.
// Ex) I own $25.00 of stock X at a cost of $20.00.
//     My gain is 25% or 2,500 basis points. To keep precision,
//     we will multiply numerator by 10,000 (* 100 to get %, and again
//     to get basis points).
pub fn delta_basis_points(delta_abs: i64, original_abs: i64) i64 {
    // If this overflows you are too rich to be using this crappy
    // software
    @setRuntimeSafety(false);
    return @divFloor(
        delta_abs * 10_000,
        original_abs,
    );
}
