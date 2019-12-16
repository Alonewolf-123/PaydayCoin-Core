Shared Libraries
================

## paydaycoinconsensus

The purpose of this library is to make the verification functionality that is critical to PaydayCoin's consensus available to other applications, e.g. to language bindings.

### API

The interface is defined in the C header `paydaycoinconsensus.h` located in `src/script/paydaycoinconsensus.h`.

#### Version

`paydaycoinconsensus_version` returns an `unsigned int` with the API version *(currently `1`)*.

#### Script Validation

`paydaycoinconsensus_verify_script` returns an `int` with the status of the verification. It will be `1` if the input script correctly spends the previous output `scriptPubKey`.

##### Parameters
- `const unsigned char *scriptPubKey` - The previous output script that encumbers spending.
- `unsigned int scriptPubKeyLen` - The number of bytes for the `scriptPubKey`.
- `const unsigned char *txTo` - The transaction with the input that is spending the previous output.
- `unsigned int txToLen` - The number of bytes for the `txTo`.
- `unsigned int nIn` - The index of the input in `txTo` that spends the `scriptPubKey`.
- `unsigned int flags` - The script validation flags *(see below)*.
- `paydaycoinconsensus_error* err` - Will have the error/success code for the operation *(see below)*.

##### Script Flags
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_NONE`
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_P2SH` - Evaluate P2SH ([BIP16](https://github.com/paydaycoin/bips/blob/master/bip-0016.mediawiki)) subscripts
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_DERSIG` - Enforce strict DER ([BIP66](https://github.com/paydaycoin/bips/blob/master/bip-0066.mediawiki)) compliance
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_NULLDUMMY` - Enforce NULLDUMMY ([BIP147](https://github.com/paydaycoin/bips/blob/master/bip-0147.mediawiki))
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY` - Enable CHECKLOCKTIMEVERIFY ([BIP65](https://github.com/paydaycoin/bips/blob/master/bip-0065.mediawiki))
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_CHECKSEQUENCEVERIFY` - Enable CHECKSEQUENCEVERIFY ([BIP112](https://github.com/paydaycoin/bips/blob/master/bip-0112.mediawiki))
- `paydaycoinconsensus_SCRIPT_FLAGS_VERIFY_WITNESS` - Enable WITNESS ([BIP141](https://github.com/paydaycoin/bips/blob/master/bip-0141.mediawiki))

##### Errors
- `paydaycoinconsensus_ERR_OK` - No errors with input parameters *(see the return value of `paydaycoinconsensus_verify_script` for the verification status)*
- `paydaycoinconsensus_ERR_TX_INDEX` - An invalid index for `txTo`
- `paydaycoinconsensus_ERR_TX_SIZE_MISMATCH` - `txToLen` did not match with the size of `txTo`
- `paydaycoinconsensus_ERR_DESERIALIZE` - An error deserializing `txTo`
- `paydaycoinconsensus_ERR_AMOUNT_REQUIRED` - Input amount is required if WITNESS is used

### Example Implementations
- [NPaydayCoin](https://github.com/NicolasDorier/NPaydayCoin/blob/master/NPaydayCoin/Script.cs#L814) (.NET Bindings)
- [node-libpaydaycoinconsensus](https://github.com/bitpay/node-libpaydaycoinconsensus) (Node.js Bindings)
- [java-libpaydaycoinconsensus](https://github.com/dexX7/java-libpaydaycoinconsensus) (Java Bindings)
- [paydaycoinconsensus-php](https://github.com/Bit-Wasp/paydaycoinconsensus-php) (PHP Bindings)
