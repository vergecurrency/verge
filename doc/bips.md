BIPs that are implemented by Verge Core (up-to-date up to **v0.17.0**):

* [`BIP 9`](https://github.com/vergecurrency/bips/blob/master/bip-0009.mediawiki): The changes allowing multiple soft-forks to be deployed in parallel have been implemented since **v0.12.1**  
* [`BIP 11`](https://github.com/vergecurrency/bips/blob/master/bip-0011.mediawiki): Multisig outputs are standard since **v0.6.0** 
* [`BIP 13`](https://github.com/vergecurrency/bips/blob/master/bip-0013.mediawiki): The address format for P2SH addresses has been implemented since **v0.6.0** 
* [`BIP 14`](https://github.com/vergecurrency/bips/blob/master/bip-0014.mediawiki): The subversion string is being used as User Agent since **v0.6.0** 
* [`BIP 16`](https://github.com/vergecurrency/bips/blob/master/bip-0016.mediawiki): The pay-to-script-hash evaluation rules have been implemented since **v0.6.0**, and took effect on *April 1st 2012* 
* [`BIP 21`](https://github.com/vergecurrency/bips/blob/master/bip-0021.mediawiki): The URI format for payments has been implemented since **v0.6.0** 
* [`BIP 22`](https://github.com/vergecurrency/bips/blob/master/bip-0022.mediawiki): The 'getblocktemplate' (GBT) RPC protocol for mining has been implemented since **v0.7.0**
* [`BIP 23`](https://github.com/vergecurrency/bips/blob/master/bip-0023.mediawiki): Some extensions to GBT have been implemented since **v0.10.0rc1**, including longpolling and block proposals 
* [`BIP 30`](https://github.com/vergecurrency/bips/blob/master/bip-0030.mediawiki): The evaluation rules to forbid creating new transactions with the same txid as previous not-fully-spent transactions were implemented since **v0.6.0**, and the rule took effect on *March 15th 2012* 
* [`BIP 31`](https://github.com/vergecurrency/bips/blob/master/bip-0031.mediawiki): The 'pong' protocol message (and the protocol version bump to 60001) has been implemented since **v0.6.1** 
* [`BIP 32`](https://github.com/vergecurrency/bips/blob/master/bip-0032.mediawiki): Hierarchical Deterministic Wallets has been implemented since **v0.13.0** 
* [`BIP 34`](https://github.com/vergecurrency/bips/blob/master/bip-0034.mediawiki): The rule that requires blocks to contain their height (number) in the coinbase input, and the introduction of version 2 blocks has been implemented since **v0.7.0**. (March 25th 2013) 
* [`BIP 35`](https://github.com/vergecurrency/bips/blob/master/bip-0035.mediawiki): The 'mempool' protocol message (and the protocol version bump to 60002) has been implemented since **v0.7.0** 
* [`BIP 37`](https://github.com/vergecurrency/bips/blob/master/bip-0037.mediawiki): The bloom filtering for transaction relaying, partial Merkle trees for blocks, and the protocol version bump to 70001 (enabling low-bandwidth SPV clients) has been implemented since **v0.8.0** 
* [`BIP 42`](https://github.com/vergecurrency/bips/blob/master/bip-0042.mediawiki): The bug that would have caused the subsidy schedule to resume after block  was fixed in **v0.9.2** 
* [`BIP 61`](https://github.com/vergecurrency/bips/blob/master/bip-0061.mediawiki): The 'reject' protocol message (and the protocol version bump to 70002) was added in **v0.9.0**  Starting *v0.17.0*, whether to send reject messages can be configured with the `-enablebip61` option.
* [`BIP 65`](https://github.com/vergecurrency/bips/blob/master/bip-0065.mediawiki): The CHECKLOCKTIMEVERIFY softfork was merged in **v0.12.0** , and backported to **v0.11.2** and **v0.10.4**. Mempool-only CLTV was added
* [`BIP 66`](https://github.com/vergecurrency/bips/blob/master/bip-0066.mediawiki): The strict DER rules and associated version 3 blocks have been implemented since **v0.10.0** 
* [`BIP 68`](https://github.com/vergecurrency/bips/blob/master/bip-0068.mediawiki): Sequence locks have been implemented as of **v0.12.1** 
* [`BIP 70`](https://github.com/vergecurrency/bips/blob/master/bip-0070.mediawiki) [`71`](https://github.com/vergecurrency/bips/blob/master/bip-0071.mediawiki) [`72`](https://github.com/vergecurrency/bips/blob/master/bip-0072.mediawiki): Payment Protocol support has been available in Verge Core GUI since **v0.9.0** 
* [`BIP 90`](https://github.com/vergecurrency/bips/blob/master/bip-0090.mediawiki): Trigger mechanism for activation of BIPs 34, 65, and 66 has been simplified to block height checks since **v0.14.0** 
* [`BIP 111`](https://github.com/vergecurrency/bips/blob/master/bip-0111.mediawiki): `NODE_BLOOM` service bit added, and enforced for all peer versions as of **v0.13.0** 
* [`BIP 112`](https://github.com/vergecurrency/bips/blob/master/bip-0112.mediawiki): The CHECKSEQUENCEVERIFY opcode has been implemented since **v0.12.1** 
* [`BIP 113`](https://github.com/vergecurrency/bips/blob/master/bip-0113.mediawiki): Median time past lock-time calculations have been implemented since **v0.12.1** 
* [`BIP 125`](https://github.com/vergecurrency/bips/blob/master/bip-0125.mediawiki): Opt-in full replace-by-fee signaling honoured in mempool and mining as of **v0.12.0** 
* [`BIP 130`](https://github.com/vergecurrency/bips/blob/master/bip-0130.mediawiki): direct headers announcement is negotiated with peer versions `>=70012` as of **v0.12.0** 
* [`BIP 133`](https://github.com/vergecurrency/bips/blob/master/bip-0133.mediawiki): feefilter messages are respected and sent for peer versions `>=70013` as of **v0.13.0** 
* [`BIP 141`](https://github.com/vergecurrency/bips/blob/master/bip-0141.mediawiki): Segregated Witness (Consensus Layer) as of **v0.13.0** , and defined for mainnet as of **v0.13.1** 
* [`BIP 143`](https://github.com/vergecurrency/bips/blob/master/bip-0143.mediawiki): Transaction Signature Verification for Version 0 Witness Program as of **v0.13.0** 
* [`BIP 144`](https://github.com/vergecurrency/bips/blob/master/bip-0144.mediawiki): Segregated Witness as of **0.13.0** 
* [`BIP 145`](https://github.com/vergecurrency/bips/blob/master/bip-0145.mediawiki): getblocktemplate updates for Segregated Witness as of **v0.13.0** 
* [`BIP 147`](https://github.com/vergecurrency/bips/blob/master/bip-0147.mediawiki): NULLDUMMY softfork as of **v0.13.1** 
* [`BIP 152`](https://github.com/vergecurrency/bips/blob/master/bip-0152.mediawiki): Compact block transfer and related optimizations are used as of **v0.13.0** 
* [`BIP 159`](https://github.com/vergecurrency/bips/blob/master/bip-0159.mediawiki): NODE_NETWORK_LIMITED service bit [signaling only] is supported as of **v0.16.0** 
* [`BIP 173`](https://github.com/vergecurrency/bips/blob/master/bip-0173.mediawiki): Bech32 addresses for native Segregated Witness outputs are supported as of **v0.16.0** 
* [`BIP 174`](https://github.com/vergecurrency/bips/blob/master/bip-0174.mediawiki): RPCs to operate on Partially Signed Transactions are present as of **v0.17.0** 
* [`BIP 176`](https://github.com/vergecurrency/bips/blob/master/bip-0176.mediawiki): Bits Denomination [QT only] is supported as of **v0.16.0**
