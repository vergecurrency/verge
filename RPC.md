# Verge Core RPC Reference

This document lists the RPC commands registered by `verged` and `verge-qt`.
Examples use `verge-cli`; JSON-RPC clients can call the same method names with the listed parameters.

Use `verge-cli help <command>` for the exact JSON result schema exposed by the running binary.

## Notes

- Secure messaging is paid-only. `smsgsend` defaults to 31 days retention and accepts either a recipient address or a shared chatkey line in the form `address-publickey`.
- Verge does not use SegWit/witness data. Some raw transaction and wallet RPC arguments retain old Bitcoin-compatible names such as `iswitness` or `address_type`; use non-witness transaction data.
- Hidden and deprecated commands are listed because they are registered RPC commands, but they are mainly for compatibility, testing, or advanced operator workflows.

## Control

| Command | Usage | Example |
| --- | --- | --- |
| `help` | `help ( "command" )` | `verge-cli help smsgsend` |
| `stop` | `stop ( "wait" )` | `verge-cli stop` |
| `uptime` | `uptime` | `verge-cli uptime` |
| `getmemoryinfo` | `getmemoryinfo ( "mode" )` | `verge-cli getmemoryinfo` |
| `logging` | `logging ( ["include",...] ["exclude",...] )` | `verge-cli logging '["net","smsg"]' '[]'` |

## Blockchain

| Command | Usage | Example |
| --- | --- | --- |
| `getblockchaininfo` | `getblockchaininfo` | `verge-cli getblockchaininfo` |
| `getchaintxstats` | `getchaintxstats ( nblocks "blockhash" )` | `verge-cli getchaintxstats 2016` |
| `getblockstats` | `getblockstats "hash_or_height" ( ["stats",...] )` | `verge-cli getblockstats 100000` |
| `getbestblockhash` | `getbestblockhash` | `verge-cli getbestblockhash` |
| `getblockcount` | `getblockcount` | `verge-cli getblockcount` |
| `getblock` | `getblock "blockhash" ( verbosity )` | `verge-cli getblock 0000000000000000000000000000000000000000000000000000000000000000 1` |
| `getblockhash` | `getblockhash height` | `verge-cli getblockhash 100000` |
| `getblockheader` | `getblockheader "blockhash" ( verbose )` | `verge-cli getblockheader 0000000000000000000000000000000000000000000000000000000000000000 true` |
| `getchaintips` | `getchaintips` | `verge-cli getchaintips` |
| `getdifficulty` | `getdifficulty` | `verge-cli getdifficulty` |
| `getmempoolancestors` | `getmempoolancestors "txid" ( verbose )` | `verge-cli getmempoolancestors <txid> true` |
| `getmempooldescendants` | `getmempooldescendants "txid" ( verbose )` | `verge-cli getmempooldescendants <txid> true` |
| `getmempoolentry` | `getmempoolentry "txid"` | `verge-cli getmempoolentry <txid>` |
| `getmempoolinfo` | `getmempoolinfo` | `verge-cli getmempoolinfo` |
| `getrawmempool` | `getrawmempool ( verbose )` | `verge-cli getrawmempool true` |
| `gettxout` | `gettxout "txid" n ( include_mempool )` | `verge-cli gettxout <txid> 0 true` |
| `gettxoutsetinfo` | `gettxoutsetinfo` | `verge-cli gettxoutsetinfo` |
| `pruneblockchain` | `pruneblockchain height` | `verge-cli pruneblockchain 100000` |
| `savemempool` | `savemempool` | `verge-cli savemempool` |
| `verifychain` | `verifychain ( checklevel nblocks )` | `verge-cli verifychain 3 6` |
| `preciousblock` | `preciousblock "blockhash"` | `verge-cli preciousblock <blockhash>` |
| `gettxoutproof` | `gettxoutproof ["txid",...] ( "blockhash" )` | `verge-cli gettxoutproof '["<txid>"]'` |
| `verifytxoutproof` | `verifytxoutproof "proof"` | `verge-cli verifytxoutproof <proof>` |

## Network

| Command | Usage | Example |
| --- | --- | --- |
| `getconnectioncount` | `getconnectioncount` | `verge-cli getconnectioncount` |
| `ping` | `ping` | `verge-cli ping` |
| `getpeerinfo` | `getpeerinfo` | `verge-cli getpeerinfo` |
| `addnode` | `addnode "node" "add|remove|onetry"` | `verge-cli addnode 127.0.0.1:20102 onetry` |
| `disconnectnode` | `disconnectnode ( "address" nodeid )` | `verge-cli disconnectnode 127.0.0.1:20102` |
| `getaddednodeinfo` | `getaddednodeinfo ( "node" )` | `verge-cli getaddednodeinfo` |
| `getnettotals` | `getnettotals` | `verge-cli getnettotals` |
| `getnetworkinfo` | `getnetworkinfo` | `verge-cli getnetworkinfo` |
| `setban` | `setban "subnet" "add|remove" ( bantime absolute )` | `verge-cli setban 192.0.2.0/24 add 86400 false` |
| `listbanned` | `listbanned` | `verge-cli listbanned` |
| `clearbanned` | `clearbanned` | `verge-cli clearbanned` |
| `setnetworkactive` | `setnetworkactive true|false` | `verge-cli setnetworkactive true` |
| `getnodeaddresses` | `getnodeaddresses ( count )` | `verge-cli getnodeaddresses 8` |

## Mining And Fees

| Command | Usage | Example |
| --- | --- | --- |
| `getnetworkhashps` | `getnetworkhashps ( nblocks height )` | `verge-cli getnetworkhashps` |
| `getallnetworkhashps` | `getallnetworkhashps ( nblocks height )` | `verge-cli getallnetworkhashps 120` |
| `getmininginfo` | `getmininginfo` | `verge-cli getmininginfo` |
| `prioritisetransaction` | `prioritisetransaction "txid" dummy fee_delta` | `verge-cli prioritisetransaction <txid> 0 10000` |
| `getblocktemplate` | `getblocktemplate ( template_request algorithm )` | `verge-cli getblocktemplate '{}' scrypt` |
| `decodeblock` | `decodeblock "hexdata"` | `verge-cli decodeblock <blockhex>` |
| `reserializeblock` | `reserializeblock "hexdata"` | `verge-cli reserializeblock <blockhex>` |
| `estimatesmartfee` | `estimatesmartfee conf_target ( "estimate_mode" )` | `verge-cli estimatesmartfee 6` |

## Raw Transactions

| Command | Usage | Example |
| --- | --- | --- |
| `getrawtransaction` | `getrawtransaction "txid" ( verbose "blockhash" )` | `verge-cli getrawtransaction <txid> true` |
| `createrawtransaction` | `createrawtransaction inputs outputs ( locktime replaceable )` | `verge-cli createrawtransaction '[{"txid":"<txid>","vout":0}]' '{"<address>":1.0}'` |
| `decoderawtransaction` | `decoderawtransaction "hexstring" ( iswitness )` | `verge-cli decoderawtransaction <txhex>` |
| `decodescript` | `decodescript "hexstring"` | `verge-cli decodescript <scripthex>` |
| `sendrawtransaction` | `sendrawtransaction "hexstring" ( allowhighfees )` | `verge-cli sendrawtransaction <txhex>` |
| `combinerawtransaction` | `combinerawtransaction ["hexstring",...]` | `verge-cli combinerawtransaction '["<txhex1>","<txhex2>"]'` |
| `signrawtransaction` | `signrawtransaction "hexstring" ( prevtxs privkeys sighashtype )` | `verge-cli signrawtransaction <txhex>` |
| `signrawtransactionwithkey` | `signrawtransactionwithkey "hexstring" ["privkey",...] ( prevtxs sighashtype )` | `verge-cli signrawtransactionwithkey <txhex> '["<privkey>"]'` |
| `signrawtransactionwithwallet` | `signrawtransactionwithwallet "hexstring" ( prevtxs sighashtype )` | `verge-cli signrawtransactionwithwallet <txhex>` |
| `testmempoolaccept` | `testmempoolaccept ["rawtx",...] ( allowhighfees )` | `verge-cli testmempoolaccept '["<txhex>"]'` |
| `fundrawtransaction` | `fundrawtransaction "hexstring" ( options iswitness )` | `verge-cli fundrawtransaction <txhex>` |

## Utility

| Command | Usage | Example |
| --- | --- | --- |
| `validateaddress` | `validateaddress "address"` | `verge-cli validateaddress <address>` |
| `createmultisig` | `createmultisig nrequired ["key",...]` | `verge-cli createmultisig 2 '["<pubkey1>","<pubkey2>"]'` |
| `verifymessage` | `verifymessage "address" "signature" "message"` | `verge-cli verifymessage <address> <signature> "hello"` |
| `signmessagewithprivkey` | `signmessagewithprivkey "privkey" "message"` | `verge-cli signmessagewithprivkey <privkey> "hello"` |
| `setalgo` | `setalgo "algo"` | `verge-cli setalgo scrypt` |
| `debuginfo` | `debuginfo` | `verge-cli debuginfo` |
| `getinfo` | `getinfo` | `verge-cli getinfo` |

## Wallet

| Command | Usage | Example |
| --- | --- | --- |
| `abandontransaction` | `abandontransaction "txid"` | `verge-cli abandontransaction <txid>` |
| `abortrescan` | `abortrescan` | `verge-cli abortrescan` |
| `addmultisigaddress` | `addmultisigaddress nrequired ["key",...] ( "label" "address_type" )` | `verge-cli addmultisigaddress 2 '["<pubkey1>","<pubkey2>"]' "vault"` |
| `backupwallet` | `backupwallet "destination"` | `verge-cli backupwallet wallet-backup.dat` |
| `bumpfee` | `bumpfee "txid" ( options )` | `verge-cli bumpfee <txid>` |
| `createwallet` | `createwallet "wallet_name"` | `verge-cli createwallet testwallet` |
| `dumpprivkey` | `dumpprivkey "address"` | `verge-cli dumpprivkey <address>` |
| `dumpwallet` | `dumpwallet "filename"` | `verge-cli dumpwallet wallet-dump.txt` |
| `encryptwallet` | `encryptwallet "passphrase"` | `verge-cli encryptwallet "change-me"` |
| `exportstealthaddress` | `exportstealthaddress "label"` | `verge-cli exportstealthaddress "private"` |
| `getaddressinfo` | `getaddressinfo "address"` | `verge-cli getaddressinfo <address>` |
| `getbalance` | `getbalance ( "dummy" minconf include_watchonly )` | `verge-cli getbalance "*" 1 false` |
| `getnewaddress` | `getnewaddress ( "label" "address_type" )` | `verge-cli getnewaddress "receiving"` |
| `getnewstealthaddress` | `getnewstealthaddress ( "label" )` | `verge-cli getnewstealthaddress "private"` |
| `getrawchangeaddress` | `getrawchangeaddress ( "address_type" )` | `verge-cli getrawchangeaddress` |
| `getreceivedbyaddress` | `getreceivedbyaddress "address" ( minconf )` | `verge-cli getreceivedbyaddress <address> 1` |
| `gettransaction` | `gettransaction "txid" ( include_watchonly )` | `verge-cli gettransaction <txid>` |
| `getunconfirmedbalance` | `getunconfirmedbalance` | `verge-cli getunconfirmedbalance` |
| `getwalletinfo` | `getwalletinfo` | `verge-cli getwalletinfo` |
| `importmulti` | `importmulti requests ( options )` | `verge-cli importmulti '[{"scriptPubKey":{"address":"<address>"},"timestamp":"now"}]'` |
| `importprivkey` | `importprivkey "privkey" ( "label" rescan )` | `verge-cli importprivkey <privkey> "imported" false` |
| `importwallet` | `importwallet "filename"` | `verge-cli importwallet wallet-dump.txt` |
| `importaddress` | `importaddress "address" ( "label" rescan p2sh )` | `verge-cli importaddress <address> "watch" false` |
| `importprunedfunds` | `importprunedfunds "rawtransaction" "txoutproof"` | `verge-cli importprunedfunds <txhex> <proof>` |
| `importpubkey` | `importpubkey "pubkey" ( "label" rescan )` | `verge-cli importpubkey <pubkey> "watch" false` |
| `importstealthaddress` | `importstealthaddress "scan_secret" "spend_secret" ( "label" )` | `verge-cli importstealthaddress <scan_secret> <spend_secret> "private"` |
| `keypoolrefill` | `keypoolrefill ( newsize )` | `verge-cli keypoolrefill 1000` |
| `listaddressgroupings` | `listaddressgroupings` | `verge-cli listaddressgroupings` |
| `listlockunspent` | `listlockunspent` | `verge-cli listlockunspent` |
| `listreceivedbyaddress` | `listreceivedbyaddress ( minconf include_empty include_watchonly address_filter )` | `verge-cli listreceivedbyaddress 1 true false` |
| `listsinceblock` | `listsinceblock ( "blockhash" target_confirmations include_watchonly include_removed )` | `verge-cli listsinceblock <blockhash> 6` |
| `liststealthaddresses` | `liststealthaddresses ( show_secrets )` | `verge-cli liststealthaddresses false` |
| `listtransactions` | `listtransactions ( "dummy" count skip include_watchonly )` | `verge-cli listtransactions "*" 10 0 false` |
| `listunspent` | `listunspent ( minconf maxconf addresses include_unsafe query_options )` | `verge-cli listunspent 1 9999999 '["<address>"]'` |
| `listwallets` | `listwallets` | `verge-cli listwallets` |
| `loadwallet` | `loadwallet "filename"` | `verge-cli loadwallet wallet.dat` |
| `lockunspent` | `lockunspent unlock ( transactions )` | `verge-cli lockunspent false '[{"txid":"<txid>","vout":0}]'` |
| `sendmany` | `sendmany "dummy" {"address":amount,...} ( minconf "comment" subtractfeefrom replaceable conf_target estimate_mode )` | `verge-cli sendmany "" '{"<address>":1.0}'` |
| `sendtoaddress` | `sendtoaddress "address" amount ( "comment" "comment_to" subtractfeefromamount replaceable conf_target estimate_mode )` | `verge-cli sendtoaddress <address> 1.0` |
| `sendtostealthaddress` | `sendtostealthaddress "address" amount ( "narration" "comment" "comment_to" )` | `verge-cli sendtostealthaddress <stealthaddress> 1.0` |
| `settxfee` | `settxfee amount` | `verge-cli settxfee 0.1` |
| `signmessage` | `signmessage "address" "message"` | `verge-cli signmessage <address> "hello"` |
| `walletlock` | `walletlock` | `verge-cli walletlock` |
| `walletpassphrasechange` | `walletpassphrasechange "oldpassphrase" "newpassphrase"` | `verge-cli walletpassphrasechange "old" "new"` |
| `walletpassphrase` | `walletpassphrase "passphrase" timeout` | `verge-cli walletpassphrase "passphrase" 60` |
| `removeprunedfunds` | `removeprunedfunds "txid"` | `verge-cli removeprunedfunds <txid>` |
| `rescanblockchain` | `rescanblockchain ( start_height stop_height )` | `verge-cli rescanblockchain 0 100000` |
| `sethdseed` | `sethdseed ( newkeypool "seed" )` | `verge-cli sethdseed true <wif_privkey>` |
| `submitblock` | `submitblock "hexdata" ( "dummy" )` | `verge-cli submitblock <blockhex>` |
| `generatetoaddress` | `generatetoaddress nblocks "address" ( maxtries )` | `verge-cli generatetoaddress 1 <address>` |
| `generate` | `generate nblocks ( maxtries )` | `verge-cli generate 1` |

## Label Wallet Commands

| Command | Usage | Example |
| --- | --- | --- |
| `getaddressesbylabel` | `getaddressesbylabel "label"` | `verge-cli getaddressesbylabel "receiving"` |
| `getreceivedbylabel` | `getreceivedbylabel "label" ( minconf )` | `verge-cli getreceivedbylabel "receiving" 1` |
| `listlabels` | `listlabels ( "purpose" )` | `verge-cli listlabels` |
| `listreceivedbylabel` | `listreceivedbylabel ( minconf include_empty include_watchonly )` | `verge-cli listreceivedbylabel 1 true false` |
| `setlabel` | `setlabel "address" "label"` | `verge-cli setlabel <address> "receiving"` |

## Secure Messaging

| Command | Usage | Example |
| --- | --- | --- |
| `smsgenable` | `smsgenable ( "walletname" )` | `verge-cli smsgenable` |
| `smsgdisable` | `smsgdisable` | `verge-cli smsgdisable` |
| `smsgoptions` | `smsgoptions ( list with_description \| set "optname" "value" )` | `verge-cli smsgoptions list true` |
| `smsglocalkeys` | `smsglocalkeys ( whitelist\|all\|wallet\|recv +/- "address"\|anon +/- "address" )` | `verge-cli smsglocalkeys all` |
| `smsgscanchain` | `smsgscanchain` | `verge-cli smsgscanchain` |
| `smsgscanbuckets` | `smsgscanbuckets` | `verge-cli smsgscanbuckets` |
| `smsginfo` | `smsginfo` | `verge-cli smsginfo` |
| `flushsmgsdb` | `flushsmgsdb` | `verge-cli flushsmgsdb` |
| `smsgaddaddress` | `smsgaddaddress "address" "pubkey"` | `verge-cli smsgaddaddress <address> <publickey>` |
| `smsgaddlocaladdress` | `smsgaddlocaladdress "address"` | `verge-cli smsgaddlocaladdress <address>` |
| `smsgimportprivkey` | `smsgimportprivkey "privkey" ( "label" )` | `verge-cli smsgimportprivkey <privkey> "chat"` |
| `smsggetpubkey` | `smsggetpubkey "address"` | `verge-cli smsggetpubkey <address>` |
| `smsgsend` | `smsgsend "address_from" "address_to|shared_chatkey" "message" ( paid_msg days_retention testfee fromfile decodehex )` | `verge-cli smsgsend <fromaddress> "<toaddress>-<publickey>" "hello" true 31` |
| `smsginbox` | `smsginbox ( "all|unread|clear" "filter" )` | `verge-cli smsginbox unread` |
| `smsgoutbox` | `smsgoutbox ( "all|clear" "filter" )` | `verge-cli smsgoutbox all` |
| `smsgbuckets` | `smsgbuckets ( stats\|dump )` | `verge-cli smsgbuckets stats` |
| `smsgview` | `smsgview` | `verge-cli smsgview` |
| `smsg` | `smsg "msgid" ( options )` | `verge-cli smsg <msgid>` |
| `smsgpurge` | `smsgpurge "msgid"` | `verge-cli smsgpurge <msgid>` |

## Hidden And Compatibility Commands

| Command | Usage | Example |
| --- | --- | --- |
| `invalidateblock` | `invalidateblock "blockhash"` | `verge-cli invalidateblock <blockhash>` |
| `reconsiderblock` | `reconsiderblock "blockhash"` | `verge-cli reconsiderblock <blockhash>` |
| `waitfornewblock` | `waitfornewblock ( timeout )` | `verge-cli waitfornewblock 60` |
| `waitforblock` | `waitforblock "blockhash" ( timeout )` | `verge-cli waitforblock <blockhash> 60` |
| `waitforblockheight` | `waitforblockheight height ( timeout )` | `verge-cli waitforblockheight 100000 60` |
| `syncwithvalidationinterfacequeue` | `syncwithvalidationinterfacequeue` | `verge-cli syncwithvalidationinterfacequeue` |
| `estimatefee` | `estimatefee nblocks` | `verge-cli estimatefee 6` |
| `estimaterawfee` | `estimaterawfee conf_target ( threshold )` | `verge-cli estimaterawfee 6` |
| `setmocktime` | `setmocktime timestamp` | `verge-cli setmocktime 0` |
| `echo` | `echo ( arg0 arg1 ... arg9 )` | `verge-cli echo hello` |
| `echojson` | `echojson ( arg0 arg1 ... arg9 )` | `verge-cli echojson '{"ok":true}'` |
| `resendwallettransactions` | `resendwallettransactions` | `verge-cli resendwallettransactions` |

## Deprecated Commands

These commands are registered for compatibility, but should not be used for new integrations.

| Command | Usage | Example |
| --- | --- | --- |
| `sendfrom` | `sendfrom "fromaccount" "toaddress" amount ( minconf "comment" "comment_to" )` | `verge-cli sendfrom "" <address> 1.0` |
| `smsgsendanon` | `smsgsendanon "address_to|shared_chatkey" "message" ( days_retention )` | `verge-cli smsgsendanon "<toaddress>-<publickey>" "hello" 31` |
| `addwitnessaddress` | `addwitnessaddress "address" ( p2sh )` | `verge-cli addwitnessaddress <address>` |
| `getaccountaddress` | `getaccountaddress "account"` | `verge-cli getaccountaddress ""` |
| `getaccount` | `getaccount "address"` | `verge-cli getaccount <address>` |
| `getaddressesbyaccount` | `getaddressesbyaccount "account"` | `verge-cli getaddressesbyaccount ""` |
| `getreceivedbyaccount` | `getreceivedbyaccount "account" ( minconf )` | `verge-cli getreceivedbyaccount "" 1` |
| `listaccounts` | `listaccounts ( minconf include_watchonly )` | `verge-cli listaccounts 1 false` |
| `listreceivedbyaccount` | `listreceivedbyaccount ( minconf include_empty include_watchonly )` | `verge-cli listreceivedbyaccount 1 true false` |
| `setaccount` | `setaccount "address" "account"` | `verge-cli setaccount <address> ""` |
| `move` | `move "fromaccount" "toaccount" amount ( minconf "comment" )` | `verge-cli move "" "savings" 1.0` |
