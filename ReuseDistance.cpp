#include <ReuseDistance.hpp>

using namespace std;

ReuseDistance::ReuseDistance(uint64_t w){
    windowsize = w;
    lastcleanup = 0;
    sequence = 0;
}

ReuseDistance::ReuseDistance(ReuseDistance& h){
    windowsize = h.GetWindowSize();
    lastcleanup = 0;
    sequence = h.GetCurrentSequence();

    vector<uint64_t> ids;
    h.GetIndices(ids);
    for (vector<uint64_t>::iterator it = ids.begin(); it != ids.end(); it++){
        uint64_t id = *it;
        ReuseStats* r = GetStats(id);

        ReuseStats* rcopy = new ReuseStats(*r);        
        stats[id] = rcopy;
    }
    ids.clear();

    vector<uint64_t> addrs;
    h.GetActiveAddresses(addrs);
    for (vector<uint64_t>::iterator it = ids.begin(); it != ids.end(); it++){
        uint64_t a = *it;
        uint64_t s = h.GetSequenceValue(a);

        if (sequence - s < windowsize){
            window[a] = s;
        }
    }
}

ReuseDistance::~ReuseDistance(){
    for (reuse_map_type<uint64_t, ReuseStats*>::iterator it = stats.begin(); it != stats.end(); it++){
        uint64_t id = it->first;
        delete stats[id];
    }
}

void ReuseDistance::GetIndices(std::vector<uint64_t>& ids){
    assert(ids.size() == 0);
    for (reuse_map_type<uint64_t, ReuseStats*>::iterator it = stats.begin(); it != stats.end(); it++){
        uint64_t id = it->first;
        ids.push_back(id);
    }
}

void ReuseDistance::GetActiveAddresses(std::vector<uint64_t>& addrs){
    assert(addrs.size() == 0);
    for (reuse_map_type<uint64_t, uint64_t>::iterator it = window.begin(); it != window.end(); it++){
        uint64_t addr = it->first;
        addrs.push_back(addr);
    }    
}

uint64_t ReuseDistance::GetSequenceValue(uint64_t a){
    if (window.count(a) == 0){
        return 0;
    }
    uint64_t s = window[a];
    if (sequence - s < windowsize){
        return s;
    }
    return 0;
}

void ReuseDistance::Print(){
    Print(cout);
}

void ReuseDistance::Print(ostream& f){
    for (reuse_map_type<uint64_t, ReuseStats*>::iterator it = stats.begin(); it != stats.end(); it++){
        uint64_t id = it->first;
        ReuseStats* r = it->second;
        
        r->Print(f, id);
    }
}

inline void ReuseDistance::Clean(){
    if (windowsize == 0){
        return;
    }
    if (sequence - lastcleanup < windowsize){
        return;
    }

    set<uint64_t> erase;
    for (reuse_map_type<uint64_t, uint64_t>::iterator it = window.begin(); it != window.end(); it++){
        uint64_t addr = (*it).first;
        uint64_t seq = (*it).second;

        if (sequence - seq >= windowsize){
            erase.insert(addr);
        }
    }

    for (set<uint64_t>::iterator it = erase.begin(); it != erase.end(); it++){
        window.erase((*it));
    }

    lastcleanup = sequence;
}

void ReuseDistance::Process(ReuseEntry* rs, uint64_t count){
    for (uint32_t i = 0; i < count; i++){
        Process(rs[i]);
    }
}

void ReuseDistance::Process(vector<ReuseEntry> rs){
    for (vector<ReuseEntry>::iterator it = rs.begin(); it != rs.end(); it++){
        ReuseEntry r = *it;
        Process(r);
    }
}

void ReuseDistance::Process(vector<ReuseEntry*> rs){
    for (vector<ReuseEntry*>::iterator it = rs.begin(); it != rs.end(); it++){
        ReuseEntry* r = *it;
        Process((*r));
    }
}

inline void ReuseDistance::Process(ReuseEntry& r){
    uint64_t addr = r.address;
    ReuseStats* s = GetStats(r.id);

    Clean();

    if (window.count(addr) == 0){
        s->Update(0);
    } else {
        assert(window.count(addr) == 1);
        uint64_t d = sequence - window[addr];
        if (windowsize && d >= windowsize){
            s->Update(0);
        } else {
            s->Update(d);
        }
    }

    window[addr] = sequence++;
}

inline ReuseStats* ReuseDistance::GetStats(uint64_t id){
    ReuseStats* s = NULL;
    if (stats.count(id) == 0){
        s = new ReuseStats();
        stats[id] = s;
    } else {
        s = stats[id];
    }
    assert(s != NULL);
    return s;
}

ReuseStats::ReuseStats(ReuseStats& r){
    vector<uint64_t> dists;
    GetSortedDistances(dists);

    for (reuse_map_type<uint64_t, uint64_t>::iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        distcounts[d] = r.CountDistance(d);
    }

    accesses = r.GetAccessCount();
}

inline uint64_t ReuseStats::GetAccessCount(){
    return accesses;
}

uint64_t ReuseStats::GetMaximumDistance(){
    uint64_t max = 0;
    for (reuse_map_type<uint64_t, uint64_t>::iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        if (d > max){
            max = d;
        }
    }
    return max;
}

inline void ReuseStats::Update(uint64_t dist){
    if (distcounts.count(dist) == 0){
        distcounts[dist] = 0;
    }
    distcounts[dist] = distcounts[dist] + 1;
    accesses++;
}

uint64_t ReuseStats::CountDistance(uint64_t d){
    if (distcounts.count(d) == 0){
        return 0;
    }
    return distcounts[d];
}

uint64_t ReuseStats::CountDistance(uint64_t l, uint64_t h){
    uint64_t t = 0;
    for (reuse_map_type<uint64_t, uint64_t>::iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        if (d >= l && d < h){
            t += distcounts[d];
        }
    }
    return t;
}

void ReuseStats::GetSortedDistances(vector<uint64_t>& dkeys){
    assert(dkeys.size() == 0 && "dkeys must be an empty vector");
    for (reuse_map_type<uint64_t, uint64_t>::iterator it = distcounts.begin(); it != distcounts.end(); it++){
        uint64_t d = it->first;
        dkeys.push_back(d);
    }
    sort(dkeys.begin(), dkeys.end());    
}

void ReuseStats::Print(ostream& f, uint64_t uniqueid){
    uint64_t outside = 0;
    if (distcounts.count(0) > 0){
        outside = distcounts[0];
    }

    f << "REUSESTATS" << TAB << dec << uniqueid << TAB << GetAccessCount() << TAB << outside << endl;

    vector<uint64_t> keys;
    GetSortedDistances(keys);

    for (vector<uint64_t>::iterator it = keys.begin(); it != keys.end(); it++){
        uint64_t d = *it;
        if (d == 0) continue;

        assert(distcounts.count(d) > 0);
        uint32_t cnt = distcounts[d];

        if (cnt > 0) f << TAB << dec << d << TAB << cnt << ENDL;
    }

}