static int calls;

int chain_value(int value) {
    calls++;
    return value + 1;
}

int chain_stock_calls(void) {
    return calls;
}

int chain_call(int value) {
    return chain_value(value);
}
