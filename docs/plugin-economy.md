# Economy API

Capability 4 uses version 1 and `SvGameEconomy`.
The table appends four methods after size and version.
All methods require the current owner callback, the server thread and a ready world.
They check context before arguments and return `SV_STATE` for invalid context.
Errors leave game records and output buffers unchanged.

## Methods

| Method | Bounds and results |
| --- | --- |
| `company(owner, company, out)` | Copy shares, float price, integer price, opening price and stake. Company indices are 0 to 4. Set the output size first. Invalid index, null output or short output returns `SV_INVALID`. |
| `set_price(owner, company, price)` | Accept a finite price of at least 10. Split at 500 or more until the price is below 500. Invalid index, price or split overflow returns `SV_INVALID`. Malformed finance state returns `SV_STATE`. |
| `set_shares(owner, company, shares)` | Accept 1 to `INT32_MAX`, with at least the sum of active player holdings. Invalid index, count or insufficient shares returns `SV_INVALID`. Malformed finance state returns `SV_STATE`. |
| `add_stocks(owner, player, amount)` | Add or remove stocks and change the company's share count by the same amount. A wrong or retired player reference returns `SV_NOT_FOUND`. Invalid team or finance state returns `SV_STATE`. A negative holding, zero total shares or overflow returns `SV_INVALID`. |

The company snapshot copies the native fields. It does not expose game pointers or account identity.
A larger output buffer is permitted. The method writes only the current record size.
Shares include unowned shares, so their count can exceed the sum of player holdings.
The API refuses a company with negative player holdings when it changes that company.
Price and stock changes also refuse holdings greater than the company's current share count.
Setting the share count can repair that condition if the new count covers all holdings.

A split multiplies the company's shares and each active member's holding by five.
It uses the coefficient and float store from `split_stock` in `economy.c`.
The API preflights every multiplication before it changes any record.
Each holding is nonnegative and no larger than the checked company total.
This also bounds each holding after the split.
The API applies repeated splits for a large price, with the same checked bound at each step.
It changes no opening price, stake or cash balance.

`set_shares` changes the total without a price change.
`add_stocks` is a grant or removal. It is not a market purchase or sale.
It charges no fee and transfers no money.
Use the money API for a separate cash change.
The existing account system still determines when player assets become saved account money.
The API adds no account field or account-file format.

## Original client

The API reuses finance event 8 and stock-price event 13.
The existing packet writer sends holdings and money as their existing 32-bit values.
It sends prices through the existing integer representation.
Fractional prices remain server values. The client receives the existing integer conversion.
Total shares and stakes receive no new protocol field.
They remain server-side market state and copied API values.
No file in `src/net` changes.
These values use packets that the game already sends. This was checked in the code, not in a client.
