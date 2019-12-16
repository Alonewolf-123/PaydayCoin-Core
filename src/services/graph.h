#ifndef PAYDAYCOIN_SERVICES_GRAPH_H
#define PAYDAYCOIN_SERVICES_GRAPH_H
#include <vector>
#include <primitives/transaction.h>

bool OrderBasedOnArrivalTime(std::vector<CTransactionRef>& blockVtx);
#endif // PAYDAYCOIN_SERVICES_GRAPH_H