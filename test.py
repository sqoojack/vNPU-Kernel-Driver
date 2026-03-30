def get_max_profit(costs, money):
    MOD = 10**9 + 7
    max_profit = 0
    remaining_money = money
    for i in range(len(costs) - 1, -1, -1):
        if remaining_money >= costs[i]:
            max_profit = (pow(2, i, MOD) + max_profit) % MOD
            remaining_money -= costs[i]
    return max_profit

        