from copy import deepcopy

# format: {other node: weight}
graph = {
    2: {3: 2, 5: 5, 18: 1},
    3: {2: 2, 5: 3, 18: 2},
    5: {2: 5, 3: 3, 18: 9, 23: 1, 45: 5},
    18: {2: 1, 3: 2, 5: 9, 23: 9},
    23: {5: 1, 18: 9, 45: 2},
    45: {5: 5, 23: 2, 49: 7},
    49: {45: 7, 60: 6},
    60: {49: 6},
}

MY_NODE = 3

# format: {dest: [cost, via neigh]}
tables = deepcopy(graph)
for (addr, table) in tables.items():
    for (dest, cost) in table.items():
        table[dest] = [cost, dest]
    table[addr] = [0, addr]


def print_tables(round):
    print('=====================================================')
    print('            ROUND', round)
    print('=====================================================')
    for (addr, table) in sorted(tables.items(), key = lambda x: x[0]):
        print(f'* {addr} *')
        for (dest, cost) in sorted(table.items(), key = lambda x: x[0]):
            print(f'{dest} : {cost}')
        print('==========================')


def print_my_table(round):
    print('=====================================================')
    print('            ROUND', round)
    print('=====================================================')
    addr, table = MY_NODE, tables[MY_NODE]
    print(f'* {addr} *')
    for (dest, cost) in sorted(table.items(), key = lambda x: x[0]):
        print(f'{dest} : {cost}')


def update_table(my_addr, my, other_addr, other) -> bool:
    changed = False
    for (dest, [cost, via]) in other.items():
        new_cost = cost + graph[my_addr][other_addr]
        if (not my.__contains__(dest)) or (my[dest][0] > new_cost):
            my[dest] = [new_cost, other_addr]
            changed = True
    return changed


round = 1
while True:
    print_my_table(round)

    changed = False
    old_tables = deepcopy(tables)
    for (addr, neighs) in graph.items():
        for neigh in neighs.keys():
            changed = update_table(addr, tables[addr], neigh, old_tables[neigh]) or changed
    
    round += 1
    if not changed:
        break