// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>

#include <assert.h>

#include <chainparamsseeds.h>
#include <arith_uint256.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion)
{
CMutableTransaction txNew;
txNew.nVersion = 1;
txNew.nTime = nTime;
txNew.vin.resize(1);
txNew.vout.resize(1);
txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
txNew.vout[0].SetEmpty();

CBlock genesis;
genesis.nTime = nTime;
genesis.nBits = nBits;
genesis.nNonce = nNonce;
genesis.nVersion = nVersion;
genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
genesis.hashPrevBlock.SetNull();
genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
return genesis;
}

/**
* Build the genesis block. Note that the output of its generation
* transaction cannot be spent since it did not originally exist in the
* database.
*/
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, bool isTestnet = false)
{
const char* pszTimestamp = isTestnet ? "VERGE TESTNET" : "Name: Dogecoin Dark";
return CreateGenesisBlock(pszTimestamp, nTime, nNonce, nBits, nVersion);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
consensus.vDeployments[d].nStartTime = nStartTime;
consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
* Main network
*/
/**
* What makes a good checkpoint block?
* + Is surrounded by blocks with reasonable timestamps
* (no blocks before with a timestamp after, none after with
* timestamp before)
* + Contains no strange transactions
*/

class CMainParams : public CChainParams {
public:
CMainParams() {
strNetworkID = "main";
consensus.ForkHeight = 2800000;
consensus.MULTI_ALGO_SWITCH_BLOCK = 340000;
consensus.STEALTH_TX_SWITCH_BLOCK = 1824150;
consensus.FlexibleMiningAlgorithms = 2042000;
consensus.CLOCK_DRIFT_FORK = 2218500;
consensus.nSubsidyHalvingInterval = 500000;

consensus.BIP34Height = consensus.ForkHeight;
consensus.BIP65Height = consensus.ForkHeight;
consensus.BIP66Height = consensus.ForkHeight;


consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000"); //ArithToUint256(~arith_uint256(0) >> 20);
consensus.nPowTargetTimespan = 30; // diff readjusting time
consensus.nPowTargetSpacing = 30; //
consensus.fPowAllowMinDifficultyBlocks = false;
consensus.fPowNoRetargeting = false;
consensus.nRuleChangeActivationThreshold = 100; // 100 out of 200 blocks
consensus.nMinerConfirmationWindow = 200;

consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

// Deployment of BIP68, BIP112, and BIP113.
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1529247969; // May 1st, 2016
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1559347200; // May 1st, 2017

// Deployment of SegWit (BIP141, BIP143, and BIP147)
// It is all safe :thumbsup:
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1529247969; // November 15th, 2016.
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1559347200; // November 15th, 2017.

// The best chain should have at least this much work.
// KeyNote: (Kind of like a checkpoint)
consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000222e000001");

// By default assume that the signatures in ancestors of this block are valid.
// KeyNote: Seems like speedup, similar to a checkpoint
consensus.defaultAssumeValid = uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc"); // block 1,100,000

/**
* The message start string is designed to be unlikely to occur in normal data.
* The characters are rarely used upper ASCII, not valid as UTF-8, and produce
* a large 32-bit integer with any alignment.
*/
pchMessageStart[0] = 0xf7;
pchMessageStart[1] = 0xa7;
pchMessageStart[2] = 0x7e;
pchMessageStart[3] = 0xff;
nDefaultPort = 21102;
nPruneAfterHeight = 100000;

genesis = CreateGenesisBlock(1412878964, 1473191, 0x1e0fffff, 1);
consensus.hashGenesisBlock = genesis.GetHash();

assert(genesis.hashMerkleRoot == uint256S("0x1c83275d9151711eec3aec37d829837cc3c2730b2bdfd00ec5e8e5dce675fd00"));
assert(consensus.hashGenesisBlock == uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278"));

// Note that of those which support the service bits prefix, most only support a subset of
// possible options.
// This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
// service bits we want, but we should get them updated to support all service bits wanted by any
// release ASAP to avoid it where possible.

// Adding some nodes in case it works
if (!gArgs.IsArgSet("-without-tor") || gArgs.GetBoolArg("-dynamic-network", false)) {
// protocol-90009 with proper TOR v3 support
vSeeds.emplace_back("xuet3co7iz2qcojnc6peuvm5gqkvn6gn6dzbvhsljurtbt2kpokxqbad.onion"); //v7.1 Swat
vSeeds.emplace_back("rg26lmazggizrw7hhesrav2mvzkvuftyaqpkpldxl6ywuscuhh3lrsyd.onion"); //v7 Swat
vSeeds.emplace_back("ractae3zucrklqxwpcu66ujbssfnrewzwdjzd5r3f3prapnpt2a2pzqd.onion"); //v7
vSeeds.emplace_back("onsldk6lgqd3panor2mg7vjlxrno7zd6oc2oij2fziqtpcma6bkwkgid.onion"); //v7
vSeeds.emplace_back("h4ystxfjtywgxymbsqlfksmp6igkvlqarmjbdoc6wul77in7geo7iiid.onion"); //v7
vSeeds.emplace_back("smfr5dlxtpo5jkjoyxdmxrbgrzgeofrmjy6kdypybjojdkuelhk3cxid.onion"); //v7
vSeeds.emplace_back("jahwfwbbdlschhhdkvyuwagotbo67kn6uo5ch6bxx5emnaomjet545yd.onion"); //v7
vSeeds.emplace_back("fs6zqnig2st24grcjdeqdvgjisohdj6itwxwqyymv7m6lku2itotczid.onion"); //v7
vSeeds.emplace_back("u4ora7mgtiu5anddbviowcsotal6rb4e5q5wtd2s7m6cwrb5m7bdezyd.onion"); //v7
vSeeds.emplace_back("k4ifangn54eqtj2si7lmj4hfujej5xtd5rctjhmefq7vcrcsnxhj3ryd.onion"); //v7
vSeeds.emplace_back("6sbjtp23eya6zx2wyirq2nzexey254mgbtx2h3avc2q2vmasnkczs6id.onion"); //v7
vSeeds.emplace_back("3cpj44g4tj64ern5kz6hv5mupbnrhq6q2dysnpifqqzvl3qynf2bjaad.onion"); //v7
vSeeds.emplace_back("nlythact7l2j2va6npqdi4fcoaf6ia5xrioujkymnkvkgsyeaextwmad.onion"); //v7
vSeeds.emplace_back("vkwwvwifgim7q77aosdiapo2pg3xl6xn47hewhp5nfnfyhb5qf6ggxqd.onion"); //v7.1
vSeeds.emplace_back("ndi3sa42bplzbtg3k5okudfbeo2wckjgyuxn7lgih6365zvhg7xkjxid.onion"); //v7.1
vSeeds.emplace_back("y6wkdtkrhdgqorzhxgfhchxb5x2i5k2zzpt4sbrdhjkydrligzuzpzqd.onion"); //v7.1
vSeeds.emplace_back("kb2bgqdwmxwxekxkc6g2s7diramjhdbiqhyxoesbzvlqsbqo7ws5l2qd.onion"); //v7
vSeeds.emplace_back("shtsxnyxjjplkfifdwoz22u7xef7os2fhaq3j7bkk5md37wj7ors4yqd.onion"); //v7
vSeeds.emplace_back("afppaj6ugeskfyjo2z24sjo37572ln7ggfkcuk3a5lkhznpqsfgxw6qd.onion"); //v7
vSeeds.emplace_back("ka2e2kaw5oxsyykw3ykdvba43eya6cotzm2huvvtrt7fw3dm62ztqfid.onion"); //v7
vSeeds.emplace_back("4wqkmkldxngbemhlebywhsjzmlqwbveksqbadcjj7ga6ctbrrcs6q3id.onion"); //v7
vSeeds.emplace_back("accu36f6dg5e54axjcytitblk4r736uenuvs6fzbp77m3akfxztpeqqd.onion"); //v7
vSeeds.emplace_back("dzp3twhnyccmyv2mchkhcihgvobgbhwj23bum7esdtg3ajyaheaxswyd.onion"); //v7
vSeeds.emplace_back("b33anagkcywi5ahfdufcvymoaioone5syhrt74bhse46hzbkcziqmfqd.onion");
vSeeds.emplace_back("rje6q245yhiyn4setn5abjlcqwapxzgwbfksrscexyhv7ffjdasmvaqd.onion");
vSeeds.emplace_back("fs6zqnig2st24grcjdeqdvgjisohdj6itwxwqyymv7m6lku2itotczid.onion");
vSeeds.emplace_back("2vrz2akwqkmbvpxa2fhxp2mdccba5253n5jqjox5zthakxrci25wnoqd.onion");
vSeeds.emplace_back("p5lmvvppkxi3jbisxaq3zojsvvnnhfowfngo6aeh63uqt66xfjjg7cyd.onion");
vSeeds.emplace_back("3o7inymtjb55o6pdpro3irptlhi447hhkxnjmuara2by2pxvlyq464id.onion");
vSeeds.emplace_back("i35eth5la6nsp55z3uyg3fawn4hk6no7pbigfbam7divbrkld7pozuid.onion");
vSeeds.emplace_back("6tick2dw6amargim7w7qauqcpnz2uzbxhyhctiyzjismsraaqehpqmad.onion");
vSeeds.emplace_back("ddnrezdtpkps456y5usfldu6ntvvcb7qir27jj362qz6pjlnkdtmnrqd.onion");
vSeeds.emplace_back("gnxxjtvdww5u3ko5a2irehzxc7f7fsccrtqkz2t6o2juflqxevznclid.onion");
vSeeds.emplace_back("5ia5cridzvmrfn6jrc7yegxdiqsn23yr3gf2b3ru54zopbv5crwt7nad.onion");
vSeeds.emplace_back("7ernyamnnp7eqqmsbmrjg36c2l3wk7bimdvlrz5qyd4c3uharwbkarad.onion");
vSeeds.emplace_back("mmybo32fgmt5fo2ttkoobbrjhm3o2jp44niwfy52vj3fydfca2eb2qid.onion");
vSeeds.emplace_back("zvbevhqr33jaz67zx4mj2xqa4uqcitlwi2egmfnmjaozbxohsqppzhqd.onion");
vSeeds.emplace_back("kdaa6uvi7thwcf4wylydy4i723t6cvoib6wpl6pikekqmea4wm2b7did.onion");
vSeeds.emplace_back("rkann5lyt2ogrvdnvlu5t7ujewonrojpypqdbp3f2mj3nxb57inxuqad.onion");
vSeeds.emplace_back("dlt3li54277xgyijen36nuaqd7oi6xu6s5ye47frjfd7g7kmnsp53kad.onion");
vSeeds.emplace_back("sufbx5fzkoirdu5vvzm53tae2zqnl3ynnxjamvcslgq62benctcaybid.onion");
vSeeds.emplace_back("n7g5wqzanuip5ito4ugrvwu64d2urahcr2g4dca6uhpdzy7bvrnflxyd.onion");
vSeeds.emplace_back("yr4q7su3pcstsfgfjedmr552rlrqmjengyzxuwt5mlz2fyjdvvu6gpad.onion");
vSeeds.emplace_back("tb2mdz7rtqiz5n6765x2arfmkizdrhmfxibhtglzo6jw72jyrn4pd2qd.onion");
vSeeds.emplace_back("ly6s7ng3a2xen7f3dnf6xrn6izigdx3xbpsp5vgtswij3lwo6gtbghyd.onion");
vSeeds.emplace_back("4tufvajf5pyuxcbzw4bvmodwxxxr3sl5ixggdn2eblycjznjfyagvhyd.onion");
vSeeds.emplace_back("fkkznbzmdpyk65swgux63fv2ybez5i4aojcxcry3oavb4qj5czvwdhad.onion");
vSeeds.emplace_back("ou46wbarsnmet3i3nsbs7zrg5bfisjrp4p7h5gemi2ricdzn6wjrxmad.onion");
vSeeds.emplace_back("r5tg55r2xnexcnk6yrzekxmv34ysqn7xvpv7vz7ebent7znppw32geyd.onion");
vSeeds.emplace_back("5tmfjfa7ftdbotoarkwbu7h7xuleahq47lumvnwdqnwoef3zbvanf3qd.onion");
vSeeds.emplace_back("cydj7pocjyvdvl3nkrb5kxow6wga24oyzfik7ycpe5xlphvlastfhqyd.onion");
vSeeds.emplace_back("segvmars637oxuy224gclsupzws7567vcuqonqx24j2iephrer2v4ryd.onion");
vSeeds.emplace_back("tjupazrghiqqotro6yzlsla3zkxe6vfc4odpozgcft4bv5a6xrpnlbqd.onion");
vSeeds.emplace_back("gaw7kjhgsfjlt4hxvxxoqlewjj7e43spzuubgcfrndglr3rmqb7kebqd.onion");
vSeeds.emplace_back("prd2sniu2n6cnv3tyu74dqvcbqsiy6ehcz2k2kiu32fkibwylzvd67qd.onion");
vSeeds.emplace_back("me42s2jffdmv2j5lq74y5dfrhjwvnh27cc7k4uleccbojcfmwfsxqzad.onion");
vSeeds.emplace_back("mlw7r6a46dpa5tlctsgfl27esjkbhhokige4knf7qonnpa7rtrfu3rid.onion");
vSeeds.emplace_back("lvmuczk2o2r65ezsqvyhofn7akax4x77ogb3awbyta6xvydz5ybtwcid.onion");
vSeeds.emplace_back("t44c5xzhrvm5eucnaobv3ynlscxil6a4fbvcmxfwcaflo6gcdc32tiyd.onion");
vSeeds.emplace_back("zqoqa6zfk6o72v4dtvtvhrhmympg7v6pntozecq6w2e6f3ycpuvkrwid.onion");
vSeeds.emplace_back("ugmkkzyyq2ztd25p33srom22uhdvflkqijz5n2rl2lzxitlhjkgyflqd.onion");
vSeeds.emplace_back("a334wu6sx5tc5kg2gxwpvqb5pcs2vgitvtiqbaksyhvviwsgasauteid.onion");
vSeeds.emplace_back("irilna4hmti5q6n7cniyqvnp3v6iq4lnjnj2vowelmkueuw7dpehq6id.onion");
vSeeds.emplace_back("p4vjqpnqslrc2gniopxgu75t2hwdjrhr56unzxyjmlvxh7l6dk33tlqd.onion");
vSeeds.emplace_back("yacufiue4c45bg2joifyuzpvl35fm3jrf7fvy2q5n7mvu22dbxum7vid.onion");
vSeeds.emplace_back("aj5ykqxwgesuruyalrlm75jvu3im6yrbm5omlzesooj4g6ojpaleziad.onion");
vSeeds.emplace_back("m7432l6hli33g2kbm4ammghqbjvgpegnhee7ycia5fve6kxjkrs4pyyd.onion");
}

if (gArgs.IsArgSet("-without-tor") || gArgs.GetBoolArg("-dynamic-network", false)) {
vSeeds.emplace_back("seed1.verge-blockchain.com"); //swat
vSeeds.emplace_back("seed2.verge-blockchain.com"); //swat
vSeeds.emplace_back("xvg.nownodes.io"); //NOWnodes seed
vSeeds.emplace_back("seed3.verge-blockchain.com"); //swat
vSeeds.emplace_back("139.59.172.93");
vSeeds.emplace_back("35.79.207.60");
vSeeds.emplace_back("34.195.53.91");
vSeeds.emplace_back("52.4.132.28");
vSeeds.emplace_back("51.178.179.75");
vSeeds.emplace_back("162.55.208.7");
vSeeds.emplace_back("54.247.115.161");
vSeeds.emplace_back("18.176.159.242");
vSeeds.emplace_back("85.106.4.146");
vSeeds.emplace_back("195.62.62.248");
vSeeds.emplace_back("185.220.101.170");
vSeeds.emplace_back("162.55.208.7");
vSeeds.emplace_back("49.12.231.33");
vSeeds.emplace_back("188.34.183.235");
}

base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,30);
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,33);
base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,158); //128 + PUBKEY_ADDRESS
base58Prefixes[EXT_PUBLIC_KEY] = {0x02, 0x2D, 0x25, 0x33};
base58Prefixes[EXT_SECRET_KEY] = {0x02, 0x21, 0x31, 0x2B};

bech32_hrp = "vg";

vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

fDefaultConsistencyChecks = false;
fRequireStandard = true;
fMineBlocksOnDemand = false;

checkpointData = {
{
{ 0, uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278")},
{ 15000, uint256S("0x000000000265c5f4683b169a68cb3cac89287c8b5df94e17b09ef19ac718026b")},
{ 100000, uint256S("0x000000000400a93131a94ad193c63faafeb8dfcc0c7d0e6f1c9c2614cb2823eb")},
{ 244219, uint256S("0x000000000139613d26f7436ecc568feb566c22d9a664359e53f0d0a542d5bdba")},
{ 400000, uint256S("0x0000000001d45af6613024ad5668bfa4909ac63e2b29c28042013d77a216830d")},
{ 500000, uint256S("0x0000000003700a4e9d81a67036d7647361086527e985cdf764648c5e61d07303")},
{ 600000, uint256S("0x30fa1eab961c99f6222f9925a27136c34ea27182c92e4f8c48ea3a90c7c2eb25")},
{ 700000, uint256S("0x3e4f3319706870bb149d1a976202c2a5e973384d181a600e7be59cbab5b63132")},
{ 800000, uint256S("0xf6b5f222bcc2f4e2439ccf6050d4ea3e9517c3752c3247302f039822ac9cc870")},
{ 900000, uint256S("0xc4d8b4079da888985854eda0200fb57045c2c70b29f10e98543f7c4076129e91")},
{1000000, uint256S("0x000000000049eaba3d6c29d9f45bc2a944b46eec005e2b038f1ee924f2f9c029")},
{1100000, uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc")},
{3700001, uint256S("0x65a6844e9cee118fd45edb78e36ed81eee149a16ccec7d859951a28ef6604cfe")}, // manifests the v6 hardfork
{4000000, uint256S("0x61dca6a2a662da0fddaa7c19aa95f09d6ffda3c0b33a4613ab3698f25b5b9552")},
{4500000, uint256S("0x180f33d62de99ed493700fa7d4607a81ca7c26a96f497670c08191a933ce28a6")},
{4765728, uint256S("0x000000000006781bb672a43ceaa683b8830faa5056cd69d6fc359a98e1e2ac8e")}, // 2/22/2021
{5039226, uint256S("0x8ee0be305788218466d9e4e174efcbd3c8a7d1368a1ae044c291e54a3ccf69fc")}, // 6/9/2021
{5296344, uint256S("0x65008bb6061b0398bee35cb8713ac90bbfc21808a4961e1edc9e3215c1ace600")}, // 10/1/2021
{5399322, uint256S("0x17ae41e30e74dfaf5e2dd3941ef71647171297fd868125f13436b1d4e45425fd")}, // 11/11/2021 (mined by Mining-Dutch.NL)
{6929661, uint256S("0x672aaffe34ac83696cce90a41ca395e07a26951197b343a0ef2f42e8d04b5407")}, //
{7000000, uint256S("0x1f1717a822c29de6514d9180cd97afc27f95c8bef23af33ddf96c3b421074949")}, // block 7milly
{7164873, uint256S("0xe265661060cbf1d33461052b0db07365217e7dec2b2e1be3f420cfbd531dfab8")}, // 10-10-23 happy 9th bday bb
{7720646, uint256S("0xee75e8bfe611f797bfd7980ca2a3ad88fbcfb2bf1c0eef905e7693e83dea851e")}, // added 9/9
{7728562, uint256S("0xd45371b7f3a9cd31b8c80eede72c380a92ac532c6777341758d7513823ab3d28")}, // added 9/12/24
{7860645, uint256S("0xc6ab28a8260fb3b50195c6891a2f36746a9850cf4869c588d99fef725e077f83")}, // added 11/12/24 found on xvg-pool.com!
}
};

checkpointIndexData = {
{
{0, uint256S("0x00000fc63692467faeb20cdb3b53200dc601d75bdfa1001463304cc790d77278")},
{100000, uint256S("0x000000000400a93131a94ad193c63faafeb8dfcc0c7d0e6f1c9c2614cb2823eb")},
{200000, uint256S("0x000000000010dd127bfe9f6b8c36080c45bcd352151aa0d2a4c79c9eba1e05b6")},
{300000, uint256S("0x00000001eeb1cbdf539452ad9ae38c3c7bb481a246dc7464d22bf6272be0bc85")},
{400000, uint256S("0x0000000001d45af6613024ad5668bfa4909ac63e2b29c28042013d77a216830d")},
{500000, uint256S("0x0000000003700a4e9d81a67036d7647361086527e985cdf764648c5e61d07303")},
{600000, uint256S("0x30fa1eab961c99f6222f9925a27136c34ea27182c92e4f8c48ea3a90c7c2eb25")},
{700000, uint256S("0x3e4f3319706870bb149d1a976202c2a5e973384d181a600e7be59cbab5b63132")},
{800000, uint256S("0xf6b5f222bcc2f4e2439ccf6050d4ea3e9517c3752c3247302f039822ac9cc870")},
{900000, uint256S("0xc4d8b4079da888985854eda0200fb57045c2c70b29f10e98543f7c4076129e91")},
{1000000, uint256S("0x000000000049eaba3d6c29d9f45bc2a944b46eec005e2b038f1ee924f2f9c029")},
{1100000, uint256S("0xc766387a2e0cd6af995ea432518614824fe313e988598ea8b26f58efb99ebcdc")},
{1200000, uint256S("0x8f35735055fbc08699daee957ec2c454ddf32a11accb24381d1b2b777dab56f9")},
{1300000, uint256S("0xc7ae4450513571d7d5c307a6bdda2ecab5bf9c4321548ab1ea352d445cab28c4")},
{1400000, uint256S("0x0f683608995591e3f78b59d1ea7b006bf689a9ea879890d717991aea7c8333bb")},
{1500000, uint256S("0xf56cdd06d6eb7b159a7348bcfb918982c1106906be1f13f67629032924105860")},
{1600000, uint256S("0x860fadffd552bd1b1c015e7d82e3bf25d8a695ac2658acaf7b171d61592888cd")},
{1700000, uint256S("0x000000000008634e0d7beb20188b807eeb7597a6038334ef1385aebe5c4daa33")},
{1800000, uint256S("0xfcfc1cb1bb408f3744f9102510c09224065a8d4046ff92e407b059bd4e5502e1")},
{1900000, uint256S("0x1033b868e71679e4e8f7e6db2b785707bbc063095b283bbc2c236fcf1c512acc")},
{2000000, uint256S("0x8457cf90dd4717a2049e737fc3e518057b0c41d61eb8b2e2a67a1707a4cec5d9")},
{2100000, uint256S("0x0000000000006034a0a84252a3de77aea8d955cd90eb7c9f090e38a21d038a42")},
{2200000, uint256S("0x00000037f9d28dfe25a6bdf681ec8f51198b0503d084f654fba72ee70f75904a")},
{2300000, uint256S("0xe4334379b84be2ac4b2118a15323cb414bedf26a1dfce2c591b33e6dca643b92")},
{2400000, uint256S("0x000000000000b9914ebfc5e69fe731572c5f110a9f9679316cb77dd5cd9e247f")},
{2500000, uint256S("0xff122f47bff95423f64d2167401192ee336a6c51b0c560afa8a9ea16ee81d886")},
{2600000, uint256S("0x20af6c9020b36967944f7dcacf4b7a49ec56c068c681f6170614585abd3de6cf")},
{2700000, uint256S("0x49fa4075fa2396bf797818e172b3b5702a39d8318606a0ee01c5d06555e65bf6")},
{3700001, uint256S("0x65a6844e9cee118fd45edb78e36ed81eee149a16ccec7d859951a28ef6604cfe")}, // manifests the v6 hardfork
{4000000, uint256S("0x61dca6a2a662da0fddaa7c19aa95f09d6ffda3c0b33a4613ab3698f25b5b9552")},
{4500000, uint256S("0x180f33d62de99ed493700fa7d4607a81ca7c26a96f497670c08191a933ce28a6")},
{4714560, uint256S("0x073cbdba83dac29384fdc8b4b07199718cb43997558925dcd4824c1e228971d6")}, // updated 2/1/2021
{4765728, uint256S("0x000000000006781bb672a43ceaa683b8830faa5056cd69d6fc359a98e1e2ac8e")}, // 2/22/2021
{5039226, uint256S("0x8ee0be305788218466d9e4e174efcbd3c8a7d1368a1ae044c291e54a3ccf69fc")}, // 6/9/2021
{5296344, uint256S("0x65008bb6061b0398bee35cb8713ac90bbfc21808a4961e1edc9e3215c1ace600")}, // 10/1/2021
{5399322, uint256S("0x17ae41e30e74dfaf5e2dd3941ef71647171297fd868125f13436b1d4e45425fd")}, // 11/11/2021 (mined by Mining-Dutch.NL)
{6929661, uint256S("0x672aaffe34ac83696cce90a41ca395e07a26951197b343a0ef2f42e8d04b5407")},
{7720646, uint256S("0xee75e8bfe611f797bfd7980ca2a3ad88fbcfb2bf1c0eef905e7693e83dea851e")}, // added 9/9
{7728562, uint256S("0xd45371b7f3a9cd31b8c80eede72c380a92ac532c6777341758d7513823ab3d28")}, // added 9/12 
{7860645, uint256S("0xc6ab28a8260fb3b50195c6891a2f36746a9850cf4869c588d99fef725e077f83")}, // added 11/12/24 found on xvg-pool.com!
}
};

chainTxData = ChainTxData{
1577552136,
8015431,
0.04004504697221486
};

// reorg
consensus.nMaxReorgDepth = 6;
consensus.nMaxReorgDepthEnforcementBlock = 4800000;

/* disable fallback fee on mainnet */
m_fallback_fee_enabled = false;
}
};

/**
* Testnet (v3)
*/
class CTestNetParams : public CChainParams {
public:
CTestNetParams() {
// KeyNote: we'll leave testnet as is for now
strNetworkID = "test";
consensus.nSubsidyHalvingInterval = 210000;
consensus.MULTI_ALGO_SWITCH_BLOCK = 340000;
consensus.STEALTH_TX_SWITCH_BLOCK = 1824150;
consensus.FlexibleMiningAlgorithms = 2042000;
consensus.CLOCK_DRIFT_FORK = 2218500;

consensus.BIP34Height = 0;
consensus.BIP65Height = 0; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
consensus.BIP66Height = 0; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000"); // FIXME: change to actual genesis limit
consensus.nPowTargetTimespan = 24 * 60 * 60; // 1 day
consensus.nPowTargetSpacing = 45;
consensus.fPowAllowMinDifficultyBlocks = true;
consensus.fPowNoRetargeting = false;
consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

// Deployment of BIP68, BIP112, and BIP113.
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

// Deployment of SegWit (BIP141, BIP143, and BIP147)
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800; // May 1st 2017

// The best chain should have at least this much work.
consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000f");

// By default assume that the signatures in ancestors of this block are valid.
consensus.defaultAssumeValid = uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"); //also genesis

pchMessageStart[0] = 0xcd;
pchMessageStart[1] = 0xf2;
pchMessageStart[2] = 0xc0;
pchMessageStart[3] = 0xef;
nDefaultPort = 21104;
nPruneAfterHeight = 1000;

genesis = CreateGenesisBlock(1462058066, 2, 0x1e0fffff, 1, true);
consensus.hashGenesisBlock = genesis.GetHash();

assert(consensus.hashGenesisBlock == uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"));
assert(genesis.hashMerkleRoot == uint256S("0x768cc22f70bbcc4de26f83aca1b4ea2a7e25f0d100497ba47c7ff2d9b696414c"));

vFixedSeeds.clear();
vSeeds.clear();
// nodes with support for servicebits filtering should be at the top
vSeeds.emplace_back("uacxdw34wnfybshfjs6hxdfzwkqxs765peu4iyyakqnz2mqyvspubeqd.onion"); // dedicated testnet server
vSeeds.emplace_back("ankgfybzzxzvu3ogo4fkopz6nfk4qv3j7gbxllawx6or5oxxjtujeyqd.onion"); // testnet blockchain explorer
vSeeds.emplace_back("2l7hxpeyhmy4c2tnlmgf7rgcn6epsaaspv7473f3r5uncqzs6pnltqqd.onion");
vSeeds.emplace_back("qwwqi7h6bkkcw6clp34ttg5mxmcwerkot2hepfhi6g4yclvlhsv2kxid.onion");
vSeeds.emplace_back("2l7hxpeyhmy4c2tnlmgf7rgcn6epsaaspv7473f3r5uncqzs6pnltqqd.onion");

base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,115);
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,198);
base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,243); // 128 + PUBKEY_ADDRESS_TEST
base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

bech32_hrp = "vt";

vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

fDefaultConsistencyChecks = false;
fRequireStandard = false;
fMineBlocksOnDemand = false;


checkpointData = {
{
{0, uint256S("65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347")},
}
};

chainTxData = ChainTxData{
// Data as of block 000000000000033cfa3c975eb83ecf2bb4aaedf68e6d279f6ed2b427c64caff9 (height 1260526)
1462058066,
1,
0.1
};

// reorg
consensus.nMaxReorgDepth = 4;
consensus.nMaxReorgDepthEnforcementBlock = 100;

/* enable fallback fee on testnet */
m_fallback_fee_enabled = true;
}
};

/**
* Regression test
*/
class CRegTestParams : public CChainParams {
public:
CRegTestParams() {
strNetworkID = "regtest";
consensus.nSubsidyHalvingInterval = 150;
consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)

consensus.ForkHeight = 2800000;
consensus.MULTI_ALGO_SWITCH_BLOCK = 340000;
consensus.STEALTH_TX_SWITCH_BLOCK = 1824150;
consensus.FlexibleMiningAlgorithms = 2042000;
consensus.CLOCK_DRIFT_FORK = 2218500;
consensus.nSubsidyHalvingInterval = 500000;
consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

consensus.nPowTargetTimespan = 7 * 24 * 60 * 60; // one week
consensus.nPowTargetSpacing = 30;
consensus.fPowAllowMinDifficultyBlocks = true;
consensus.fPowNoRetargeting = true;
consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

// The best chain should have at least this much work.
consensus.nMinimumChainWork = uint256S("0x00");

// By default assume that the signatures in ancestors of this block are valid.
consensus.defaultAssumeValid = uint256S("0x00");

pchMessageStart[0] = 0xfa;
pchMessageStart[1] = 0xbf;
pchMessageStart[2] = 0xb5;
pchMessageStart[3] = 0xda;
nDefaultPort = 31102;
nPruneAfterHeight = 1000;

genesis = CreateGenesisBlock(1462058066, 2, 0x1e0fffff, 1, true);
consensus.hashGenesisBlock = genesis.GetHash();

assert(consensus.hashGenesisBlock == uint256S("0x65b4e101cacf3e1e4f3a9237e3a74ffd1186e595d8b78fa8ea22c21ef5bf9347"));
assert(genesis.hashMerkleRoot == uint256S("0x768cc22f70bbcc4de26f83aca1b4ea2a7e25f0d100497ba47c7ff2d9b696414c"));

vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
vSeeds.clear(); //!< Regtest mode doesn't have any DNS seeds.

fDefaultConsistencyChecks = true;
fRequireStandard = false;
fMineBlocksOnDemand = true;

checkpointData = {
{}
};

chainTxData = ChainTxData{
0,
0,
0
};

base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
base58Prefixes[SECRET_KEY] = std::vector<unsigned char>(1,239);
base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

bech32_hrp = "vgrt";

// reorg
consensus.nMaxReorgDepth = 4;
consensus.nMaxReorgDepthEnforcementBlock = 100;

/* enable fallback fee on regtest */
m_fallback_fee_enabled = true;
}
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
assert(globalChainParams);
return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
if (chain == CBaseChainParams::MAIN)
return std::unique_ptr<CChainParams>(new CMainParams());
else if (chain == CBaseChainParams::TESTNET)
return std::unique_ptr<CChainParams>(new CTestNetParams());
else if (chain == CBaseChainParams::REGTEST)
return std::unique_ptr<CChainParams>(new CRegTestParams());
throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
SelectBaseParams(network);
globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
