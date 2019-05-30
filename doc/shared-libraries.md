Shared Libraries
================

## vergeconsensus

The purpose of this library is to make the verification functionality that is critical to Verge's consensus available to other applications, e.g. to language bindings.

### API

The interface is defined in the C header `vergeconsensus.h` located in  `src/script/vergeconsensus.h`.

#### Version

`vergeconsensus_version` returns an `unsigned int` with the API version *(currently at an experimental `0`)*.

#### Script Validation

`vergeconsensus_verify_script` returns an `int` with the status of the verification. It will be `1` if the input script correctly spends the previous output `scriptPubKey`.

##### Parameters
- `const unsigned char *scriptPubKey` - The previous output script that encumbers spending.
- `unsigned int scriptPubKeyLen` - The number of bytes for the `scriptPubKey`.
- `const unsigned char *txTo` - The transaction with the input that is spending the previous output.
- `unsigned int txToLen` - The number of bytes for the `txTo`.
- `unsigned int nIn` - The index of the input in `txTo` that spends the `scriptPubKey`.
- `unsigned int flags` - The script validation flags *(see below)*.
- `vergeconsensus_error* err` - Will have the error/success code for the operation *(see below)*.

##### Errors
- `vergeconsensus_ERR_OK` - No errors with input parameters *(see the return value of `vergeconsensus_verify_script` for the verification status)*
- `vergeconsensus_ERR_TX_INDEX` - An invalid index for `txTo`
- `vergeconsensus_ERR_TX_SIZE_MISMATCH` - `txToLen` did not match with the size of `txTo`
- `vergeconsensus_ERR_DESERIALIZE` - An error deserializing `txTo`
- `vergeconsensus_ERR_AMOUNT_REQUIRED` - Input amount is required if WITNESS is used

### Example Implementations
- [NVerge](https://github.com/NicolasDorier/NVerge/blob/master/NVerge/Script.cs#L814) (.NET Bindings)
- [node-libvergeconsensus](https://github.com/bitpay/node-libvergeconsensus) (Node.js Bindings)
- [java-libvergeconsensus](https://github.com/dexX7/java-libvergeconsensus) (Java Bindings)
- [vergeconsensus-php](https://github.com/Bit-Wasp/vergeconsensus-php) (PHP Bindings)
