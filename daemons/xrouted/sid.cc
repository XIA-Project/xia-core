process_msg
{
		case Xroute::SID_MANAGE_KA_MSG:
			rc = processServiceKeepAlive(msg.sid_keep_alive());
			break;
		case Xroute::SID_TABLE_UPDATE_MSG:
		//////////////////////////////////////////////////////////////////////
		// not sure if these should be running again
		case Xroute::SID_DISCOVERY_MSG:
//			rc = processSidDiscovery(msg.sid_discovery());
			break;

		case Xroute::SID_DECISION_QUERY_MSG:
//			rc = processSidDecisionQuery(msg.sid_query());
			break;

		case Xroute::SID_DECISION_ANSWER_MSG:
//			rc = processSidDecisionAnswer(msg.sid_answer());
			break;

		//////////////////////////////////////////////////////////////////////
		// not sure these even exist
		case Xroute::SID_RE_DISCOVERY_MSG:
		case Xroute::AD_PATH_STATE_PING_MSG:
		case Xroute::AD_PATH_STATE_PONG_MSG:
			syslog(LOG_INFO, "controller received a limbo message");
			break;
}
purge()
{
	if (now - _last_update_config >= _settings->update_config()) {
		_last_update_config = now;
		set_sid_conf(_hostname);
	}
	if (now - _last_update_latency >= _settings->update_latency()) {
		_last_update_latency = now;
		updateADPathStates(); // update latency info
	}
}


handler()
{
	if (timercmp(&now, &sd_fire, >=)) {
//		sendSidDiscovery();
		timeradd(&now, &sd_freq, &sd_fire);
	}
	if (timercmp(&now, &sq_fire, >=)) {		if (_settings->enable_sid_ctl()) {
//			querySidDecision();
		}
		timeradd(&now, &sq_freq, &sq_fire);
	}
}


init()
{
	sd_freq.tv_sec = 3;
	sd_freq.tv_usec = 0;
	sq_freq.tv_sec = 5;
	sq_freq.tv_usec = 0;
}

int Controller::processSidDiscovery(const Xroute::SIDDiscoveryMsg& msg)
{
	//TODO: add version(time-stamp?) for each entry
	int rc = 1;

	Xroute::XID a = msg.ad();
	Xroute::XID h = msg.hid();
	string srcAD  = Node(a.type(), a.id().c_str(), 0).to_string();
	string srcHID = Node(h.type(), h.id().c_str(), 0).to_string();

	//syslog(LOG_INFO, "Get SID discovery msg from %s", srcAD.c_str());

	// process the entries: AD-SID pairs
	//syslog(LOG_INFO, "Get %d origin SIDs", records);
	for (int i = 0; i < msg.entries_size(); ++i)
	{
		ServiceState service_state;
		Xroute::DiscoveryEntry d = msg.entries(i);

		Xroute::XID x = d.ad();
		string AD = Node(x.type(), x.id().c_str(), 0).to_string();

		x = d.sid();
		string SID = Node(x.type(), x.id().c_str(), 0).to_string();

		//TODO: read the parameters from msg, put them into service_state
		service_state.capacity = d.capacity();
		service_state.capacity_factor = d.capacity_factor();
		service_state.link_factor = d.link_factor();
		service_state.priority = d.priority();
		//syslog(LOG_INFO, "Get broadcast SID %s@%s, p= %d,s=%d",SID.c_str(), AD.c_str(), priority, seq);
		service_state.leaderAddr = d.leader_addr();
		service_state.archType = d.arch_type();
		service_state.seq = d.seq();
		service_state.percentage = 0;
		updateSidAdsTable(AD, SID, service_state);
	}

	//syslog(LOG_INFO, "SID-ADs %lu", _SIDADsTable.size());
	// rc = processSidDecision(); // use a timeout callback instead?
	// TODO: read the config file frequently, emulate service failure
	// TODO: how/when to revoke/remove a entry
	// TODO: TTL for AD-SID pair for fast failure discovery/recovery
	return rc;
}

int Controller::querySidDecision()
{
	// New design. Report local information to sid controller. Then wait for answer.
	//syslog(LOG_DEBUG, "Sending SID decision queries for %lu SIDs", _SIDADsTable.size() );
	int rc = 1;
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;

	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		// for each SID, generate a report packet.
		// packet format: SID/rate/#options/optionAD1/Latency1,capacity1/optionAD2/Latency2...

		//syslmog(LOG_DEBUG, "Sending SID decision queries for %s", it_sid->first.c_str() );

		Node ad(_myAD);
		Node hid(_myHID);
		Node sid(it_sid->first);

		Xroute::XrouteMsg msg;
		Xroute::DecisionQueryMsg *q = msg.mutable_sid_query();
		Xroute::XID              *a  = q ->mutable_ad();
		Xroute::XID              *h  = q ->mutable_hid();
		Xroute::XID              *s  = q ->mutable_sid();

		msg.set_type(Xroute::SID_DECISION_QUERY_MSG);
		msg.set_version(Xroute::XROUTE_PROTO_VERSION);
		a->set_type(ad.type());
		a->set_id(ad.id(), XID_SIZE);
		h->set_type(hid.type());
		h->set_id(hid.id(), XID_SIZE);
		s->set_type(sid.type());
		s->set_id(sid.id(), XID_SIZE);


		int rate = 0;
		if (_SIDRateTable.find(it_sid->first) != _SIDRateTable.end()) {
			rate = _SIDRateTable[it_sid->first];
		}
		if (rate < 0) { // it should not happen
			rate = 0;
		}

		q->set_rate(rate);

		std::map<std::string, ServiceState>::iterator it_ad;
		std::string best_ad; // the cloest controller
		int minimal_latency = 9999; // smallest latency

		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{ // second pass, find the cloest one, creat the message
			if (it_ad->second.priority < 0) { // this is the poisoned one, skip
				continue;
			}
			int latency = 9998; // default, 9999-1ms
			if (_ADPathStates.find(it_ad->first) != _ADPathStates.end()) {
				latency = _ADPathStates[it_ad->first].delay;
			}
			minimal_latency = minimal_latency > latency?latency:minimal_latency;
			best_ad = minimal_latency == latency?it_ad->first:best_ad; // find the cloest

			Node aa(it_ad->first);

			Xroute::QueryEntry *e = q->add_ads();
			a = e->mutable_ad();
			a->set_type(aa.type());
			a->set_id(aa.id(), XID_SIZE);
			e->set_latency(latency);
			e->set_capacity(it_ad->second.capacity); //capacity
		}
		//syslog(LOG_DEBUG, "Going to send to %s", best_ad.c_str() );

		// send the msg
		if (best_ad == _myAD) { // I'm the one
			//syslog(LOG_DEBUG, "Sending SID decision query locally");
			processSidDecisionQuery(msg); // process locally

		} else {

			string message;
			sockaddr_x ddag;
			Graph g = Node() * Node(best_ad) * Node(controller_sid);
			g.fill_sockaddr(&ddag);

			msg.SerializeToString(&message);
			int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
			if (temprc < 0) {
				syslog(LOG_ERR, "error sending SID decision query to %s", best_ad.c_str());
			}
			rc = (temprc < rc)? temprc : rc;
			//syslog(LOG_DEBUG, "sent SID %s decision query to %s", it_sid->first.c_str(), best_ad.c_str());
		}
	}
	return rc;
}

int Controller::processSidDecisionQuery(const Xroute::XrouteMsg& msg)
{
	// work out a decision for each query
	// TODO: This could/should be async
	//syslog(LOG_DEBUG, "Processing SID decision query");
	int rc = 1;

	Xroute::XID xad  = msg.sid_query().ad();
	Xroute::XID xhid = msg.sid_query().hid();
	Xroute::XID xsid = msg.sid_query().sid();

	string srcAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	string srcHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	string SID    = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	uint32_t rate = msg.sid_query().rate();

	//syslog(LOG_INFO, "Get %s query msg from %s, rate %d",SID.c_str(), srcAD.c_str(), rate);

	// process the entries: AD-latency pairs
	// check if I am the SID controller
	std::map<std::string, ServiceState>::iterator it_sid;
	it_sid = _LocalSidList.find(SID);
	if (it_sid == _LocalSidList.end()) {
		// not found, I'm not the right controller to talk to
		syslog(LOG_INFO, "I'm not the controller for %s", SID.c_str());
		// TODO: reply error msg to the source - return???
	}

	if (it_sid->second.archType == ARCH_CENT && !it_sid->second.isLeader) { // should forward to the leader

		// are we forging src addr?
		sockaddr_x ddag;
		string message;
		//syslog(LOG_INFO, "Send forward query %s", it_sid->second.leaderAddr.c_str());
		Graph g(it_sid->second.leaderAddr);
		g.fill_sockaddr(&ddag);

		msg.SerializeToString(&message);
		rc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
		if (rc < 0) {
			syslog(LOG_ERR, "error sending SID query forwarding to %s", it_sid->second.leaderAddr.c_str());
		}
		return rc;

	} else {
		std::map<std::string, DecisionIO> decisions;
		for (int i = 0; i < msg.sid_query().ads_size(); ++i) {
			DecisionIO dio;

			Xroute::QueryEntry q = msg.sid_query().ads(i);
			Xroute::XID a = q.ad();

			string ad = Node(a.type(), a.id().c_str(), 0).to_string();

			dio.capacity   = q.capacity();
			dio.latency    = q.latency();
			dio.percentage = 0;
			decisions[ad] = dio;
			//syslog(LOG_INFO, "Get %d ms for %s", latency, AD.c_str());
		}
		// compute the weights
		it_sid->second.decision(SID, srcAD, rate, &decisions);

		//reply to the query
		Node aa(_myAD);
		Node hh(_myHID);

		Xroute::XID *x;
		Xroute::XrouteMsg xm;
		Xroute::DecisionAnswerMsg *dam = xm.mutable_sid_answer();

		xm.set_type(Xroute::SID_DECISION_ANSWER_MSG);
		xm.set_version(Xroute::XROUTE_PROTO_VERSION);


		x = dam->mutable_ad();
		x->set_type(aa.type());
		x->set_id(aa.id(), XID_SIZE);

		x = dam->mutable_hid();
		x->set_type(hh.type());
		x->set_id(hh.id(), XID_SIZE);

		x = dam->mutable_sid();
		x->set_type(xsid.type());
		x->set_id(xsid.id().c_str(), XID_SIZE);

		std::map<std::string, DecisionIO>::iterator it_ds;
		for (it_ds = decisions.begin(); it_ds != decisions.end(); ++it_ds) {
			Xroute::QueryAnswer *qa = dam->add_sids();

			Node xx(it_ds->first);
			Xroute::XID *xa = qa->mutable_ad();
			xa->set_type(xx.type());
			xa->set_id(xx.id(), XID_SIZE);

			qa->set_percentage(it_ds->second.percentage);
		}

		// send the msg
		if (srcAD == _myAD) { // I'm the one
			//syslog(LOG_DEBUG, "Sending SID decision locally");
			processSidDecisionAnswer(xm.sid_answer()); // process locally
		} else {
			string message;
			sockaddr_x ddag;
			Graph g = Node() * Node(srcAD) * Node(controller_sid);
			g.fill_sockaddr(&ddag);

			msg.SerializeToString(&message);
			rc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&ddag, sizeof(sockaddr_x));
			if (rc < 0) {
				syslog(LOG_ERR, "error sending SID decision answer to %s", srcAD.c_str());
			}
			//syslog(LOG_DEBUG, "sent SID %s decision answer to %s", SID.c_str(), srcAD.c_str());
		}

		return rc;
	}
}

int Controller::Latency_first(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{
	//syslog(LOG_DEBUG, "Decision function %s: Latency_first for %s", SID.c_str(), srcAD.c_str());
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	string best_ad;
	std::map<std::string, DecisionIO>::iterator it_ad;
	int minimal_latency = 9999; // smallest latency

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // first pass, just find the minimal latency
		// e2e latency + internal delay
		int delay = it_ad->second.latency;
		if ( _LocalServiceLeaders[SID].instances.find(it_ad->first) != _LocalServiceLeaders[SID].instances.end()) {
			delay += _LocalServiceLeaders[SID].instances[it_ad->first].internal_delay;
		}
		minimal_latency = minimal_latency > delay?delay:minimal_latency;
		best_ad = minimal_latency == delay?it_ad->first:best_ad; // find the closest
	}

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // second pass, assign weight
		if (it_ad->first ==  best_ad) {
			it_ad->second.percentage = 100;
		} else {
			it_ad->second.percentage = 0;
		}
	}
	return 0;
}

int Controller::Load_balance(std::string, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{
	//syslog(LOG_DEBUG, "Decision function %s: load balance for %s", SID.c_str(), srcAD.c_str());
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	if (decision == NULL || decision->empty()) {
		syslog(LOG_INFO, "Get 0 entries to decide!");
		return -1;
	}
	std::map<std::string, DecisionIO>::iterator it_ad;
	int weight = 0;

	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // first pass, count capacity
		weight += it_ad->second.capacity;
	}
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // second pass, assign weight
		it_ad->second.percentage = 100.0 * it_ad->second.capacity / weight + 0.5; //round
	}

	return 0;
}

bool Controller::compareCL(const ClientLatency &a, const ClientLatency &b)
{
	return a.latency > b.latency;
}


int Controller::Rate_load_balance(std::string SID, std::string srcAD, int rate, std::map<std::string, DecisionIO>* decision)
{ // balance the load depending on the capacity of replicas and traffic rate of client domains
	if (srcAD == "" || rate < 0) {
		syslog(LOG_INFO, "Error parameters");
		return -1;
	}
	if (decision == NULL || decision->empty()) {
		syslog(LOG_INFO, "Get 0 entries to decide!");
		return -1;
	}
	//update rate first
	_LocalServiceLeaders[SID].rates[srcAD] = rate;
	std::map<std::string, int> rates = _LocalServiceLeaders[SID].rates;
	std::map<std::string, int>::iterator dumpit;

	/*mark the 0 rate client as -1*/
	for (dumpit = rates.begin(); dumpit != rates.end(); ++dumpit) {
		if (dumpit->second == 0) {
			dumpit->second = -1;
		}
	}

	std::map<std::string, int> capacities;

	// update latency
	std::map<std::string, DecisionIO>::iterator it_ad;
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // update latency
		_LocalServiceLeaders[SID].latencies[it_ad->first][srcAD] = it_ad->second.latency;
		capacities[it_ad->first] = it_ad->second.capacity;
		//syslog(LOG_DEBUG, "record capacity %s %d", it_ad->first.c_str(), it_ad->second.capacity);
	}

	// compute the weight

	//first, convert to lists sorted by latency
	std::map<std::string, std::vector<ClientLatency> > latency_map;
	std::map<std::string, std::map<std::string, int> >::iterator it = _LocalServiceLeaders[SID].latencies.begin();
	for (; it != _LocalServiceLeaders[SID].latencies.end(); ++it) {
		//syslog(LOG_DEBUG, "convert %s", it->first.c_str());

		std::vector<ClientLatency> cls;
		std::map<std::string, int>::iterator it2 = it->second.begin();
		for ( ; it2 != it->second.end(); ++it2) {
			//syslog(LOG_DEBUG, "convert2 %s %d", it2->first.c_str(), it2->second);
			ClientLatency cl;
			cl.AD = it2->first;
			cl.latency = it2->second;
			cls.push_back(cl);
		}
		std::sort(cls.begin(), cls.end(), compareCL);
		latency_map[it->first] = cls;
	}

	//second, heuristic allocation, allocate full capacity to the client-replica pair with the smallest latency, then repeat.

	std::map<std::string, std::vector<ClientLatency> >::iterator it_lp;
	std::map<std::string, std::vector<ClientLatency> >::iterator it_best;
	while (1) {
		/*find the pair*/
		it_best = latency_map.end();
		std::string AD;
		for (it_lp = latency_map.begin(); it_lp != latency_map.end(); ++it_lp) {
			/*clean up*/
			if (it_lp->second.empty()) { // this list is empty
				continue;
			}
			if (capacities[it_lp->first] <= 0) {
				while (!it_lp->second.empty()) { // no more capacity, clean up
					AD = it_lp->second.back().AD;
					if (rates[AD] >= 0) { // have demand
						it_lp->second.pop_back(); // remove it
					} else {
						//keep the -1 item, stop cleaning
						break;
					}
				}
			}
			while(1) { // find one that need to be allocated
				if (it_lp->second.empty()) { // this list is empty
					break;
				}
				AD = it_lp->second.back().AD;
				if (rates.find(AD) == rates.end() || rates[AD] == 0) { // already allocated this client
					it_lp->second.pop_back();
					continue;
				} else {
					break;
				}
			}
			if (it_lp->second.empty()) { // this list is empty
				continue;
			}
			/*find the smallest one*/
			if (it_best == latency_map.end() || it_best->second.back().latency > it_lp->second.back().latency) {
				it_best = it_lp;
			}
		}
		if (it_best == latency_map.end()) { // nothing left
			//syslog(LOG_ERR, "allocation done");
			break;
		} else {
			//syslog(LOG_ERR, "find best %s %d", it_best->second.back().AD.c_str(), it_best->second.back().latency);
		}
		/*allocate max capacity of the corresponding replica to the client with the smallest latency*/
		// we do this for all the client but only record the allocation for srcAD
		AD = it_best->second.back().AD;
		std::string replica = it_best->first;
		if (rates[AD] == -1) { // this is a zero request rate one, just allocate it to the closest replica
			if (AD == srcAD) {
				(*decision)[replica].percentage = 100; // any value
				//syslog(LOG_ERR, "allocate0 100%%: for %s", srcAD.c_str());
			}
			rates[AD] = 0; //mark it done
			it_best->second.pop_back();

		} else if (rates[AD] >= capacities[replica]) { // allocate the remaining capacity of that replica
			rates[AD] -= capacities[replica];
			it_best->second.pop_back();
			if (AD == srcAD) {
				(*decision)[replica].percentage = capacities[replica];
				//syslog(LOG_ERR, "allocate1 %d: for %s", capacities[replica], srcAD.c_str());
			}
			capacities[replica] = 0;
			while (!it_best->second.empty()) { // no more capacity, clean up
				AD = it_best->second.back().AD;
				if (rates[AD] >= 0) { // have demand
					it_best->second.pop_back(); // remove it
				} else {
					//keep the -1 item, stop cleaning
					break;
				}
			}
			//it_best->second.clear();
		} else { // capacity > rates[AD]
			capacities[replica] -= rates[AD];
			if (AD == srcAD) {
				(*decision)[replica].percentage = rates[AD];
				//syslog(LOG_ERR, "allocate2 %d: for %s", rates[AD], srcAD.c_str());

			}
			rates[AD] = 0;
			it_best->second.pop_back();
		}
	}
	//syslog(LOG_DEBUG, "Finish R_LB");

	//normalize the percentage
	int sum = 0;
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // compute the sum
		sum += it_ad->second.percentage;
		//syslog(LOG_DEBUG, "record percentage %s %d", it_ad->first.c_str(), it_ad->second.percentage);
	}
	if (sum == 0) {
		syslog(LOG_ERR, "R_LB: sum = 0!");
		return -1;
	}
	for (it_ad = decision->begin(); it_ad != decision->end(); ++it_ad)
	{ // update percentage
		it_ad->second.percentage = 100.0 * it_ad->second.percentage / sum + 0.5; // +0.5 to round
	}

	return 0;
}

int Controller::processSidDecisionAnswer(const Xroute::DecisionAnswerMsg& msg)
{
	// When got the answer, set local weight
	// The answer is per SID, when to update routing table?

	//syslog(LOG_DEBUG, "Processing SID decision");
	if (!ENABLE_SID_CTL) {
		// if SID control plane is disabled
		// we should not get such a ctl message
		return 0;
	}

	Xroute::XID xad  = msg.ad();
	Xroute::XID xhid = msg.hid();
	Xroute::XID xsid = msg.sid();

	string srcAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	string srcHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	string SID    = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	//syslog(LOG_INFO, "Get %s, %d answer msg from %s", SID.c_str(), records, srcAD.c_str());

	// process the entries: AD-latency pairs
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	it_sid = _SIDADsTable.find(SID);
	if (it_sid == _SIDADsTable.end()) {
		syslog(LOG_ERR, "No record for %s", SID.c_str());
		return -1;
	}

	std::map<std::string, ServiceState>::iterator it_ad;
	// check the to-be-deleted entries first
	for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad) {
		if (it_ad->second.priority < 0) {
			it_ad->second.percentage = -1;
		}
	}

	for (int i = 0; i < msg.sids_size(); i++) {
		Xroute::QueryAnswer q = msg.sids(i);

		xad = q.ad();
		string AD = Node(xad.type(),  xad.id().c_str(),  0).to_string();

		uint32_t percentage = q.percentage();

		//syslog(LOG_INFO, "Get %s, %d %%", AD.c_str(), percentage);
		std::map<std::string, ServiceState>::iterator it_ad = it_sid->second.find(AD);

		it_ad->second.valid = true; // valid is not implemented yet
		if (it_ad == it_sid->second.end()) {
			syslog(LOG_ERR, "No record for %s@%s", SID.c_str(), AD.c_str());
		} else {
			it_ad->second.percentage = percentage;
			if (percentage > 100) {
				syslog(LOG_ERR, "Invalid weight: %d", percentage);
			}
			if (it_ad->second.priority < 0) {
				syslog(LOG_ALERT, "To-be-deleted record received, %s@%s, it's OK", SID.c_str(), AD.c_str());
			}
		}
	}
/*
	for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad) {
		syslog(LOG_INFO, "DEBUG: %s weight: %d",it_ad->first.c_str(), it_ad->second.percentage );
	}
*/
	// for debug
	// dumpSidAdsTable();

	// update every router
	// temporally put it here.
	//sendSidRoutingDecision();

	return 0;
}


int Controller::processSidDecision(void) // to be deleted
{
	// make decision based on principles like highest priority first, load balancing, nearest...
	// Using function: (capacity^factor/link^factor)*priority for weight
	int rc = 1;

	//local balance decision: decide which percentage of traffic each AD should has
	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		// calculate the percentage for each AD for this SID
		double total_weight = 0;
		int minimal_latency = 9998;

		std::map<std::string, ServiceState>::iterator it_ad;
		std::string best_ad;
		// find the total weight
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.priority < 0) { // this is the poisoned one, skip
				continue;
			}
			int latency = 9999; // default, 9999ms
			if (_ADPathStates.find(it_ad->first) != _ADPathStates.end()) {
				latency = _ADPathStates[it_ad->first].delay;
			}
			if (it_ad->second.capacity_factor == 0) { // only latency matters find the smallest
				minimal_latency = minimal_latency > latency?latency:minimal_latency;
				best_ad = minimal_latency == latency?it_ad->first:best_ad;
			}
			it_ad->second.weight = pow(it_ad->second.capacity, it_ad->second.capacity_factor)/
								   pow(latency, it_ad->second.link_factor)*it_ad->second.priority;
			total_weight += it_ad->second.weight;
			//syslog(LOG_INFO, "%s @%s :cap=%d, f=%d, late=%d, f=%d, prio=%d, weight is %f",it_sid->first.c_str(), it_ad->first.c_str(), it_ad->second.capacity, it_ad->second.capacity_factor, latency, it_ad->second.link_factor, it_ad->second.priority, it_ad->second.weight );
		}
			//syslog(LOG_INFO, "total_weight is %f", total_weight );
		// make local decision. map weights to 0..100
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.priority < 0) { // this is the poisoned one
				//syslog(LOG_DEBUG, "Prepare to delete %s@%s\n", it_sid->first.c_str(), it_ad->first.c_str());
				it_ad->second.valid = true;
				it_ad->second.percentage = -1; //mark it to be deleted

			} else {
				it_ad->second.valid = true;
				if (it_ad->second.capacity_factor == 0) {// only latency matters
					it_ad->second.percentage = best_ad == it_ad->first?100:0; // go to the closest

				} else { // normal
					it_ad->second.percentage = (int) (100.0*it_ad->second.weight/total_weight+0.5);//round
				}
				if (it_ad->second.percentage > 100) {
					syslog(LOG_INFO, "Error setting weight%s @%s :cap=%d, f=%d, prio=%d, weight=%f, total weight=%f",it_sid->first.c_str(), it_ad->first.c_str(), it_ad->second.capacity, it_ad->second.capacity_factor, it_ad->second.priority, it_ad->second.weight, total_weight);
				}
			}
			//syslog(LOG_INFO, "percentage for %s@%s is %d",it_sid->first.c_str(),it_ad->first.c_str(), it_ad->second.percentage );
		}
	}
	// for debug
	// dumpSidAdsTable();

	// update every router
	//sendSidRoutingDecision();
	return rc;
}

int Controller::sendSidRoutingDecision(void)
{
	syslog(LOG_INFO, "Controller::Send-SID Decision");

	// TODO: when to call this function timeout? new incoming sid discovery?
	// TODO: if not reusing AD routing table, this function should calculate routing table
	// for each router
	// Now we just send the identical decision to every router. The routers will reuse their
	// own routing table to interpret the decision
	//syslog(LOG_DEBUG, "Processing SID decision routes");
	int rc = 1;

	// remap the SIDADsTable
	std::map<std::string, std::map<std::string, ServiceState> > ADSIDsTable;

	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
	for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
	{
		std::map<std::string, ServiceState>::iterator it_ad;
		for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
		{
			if (it_ad->second.valid) // only send choices we want to use
			{
				if (ADSIDsTable.count(it_ad->first) > 0) // has key AD
				{
					ADSIDsTable[it_ad->first][it_sid->first] = it_ad->second;

				} else {
				// create a new map for new AD
					std::map<std::string, ServiceState> new_map;
					new_map[it_sid->first] = it_ad->second;
					ADSIDsTable[it_ad->first] = new_map;
				}
			}
		}
	}

	// for each router:
	std::map<std::string, NodeStateEntry>::iterator it_router;
	for (it_router = _networkTable.begin(); it_router != _networkTable.end(); ++it_router)
	{
		if ((it_router->second.ad != _myAD) || (it_router->second.hid == ""))
		{
			// Don't calculate routes for external ADs
			continue;
		} else if (it_router->second.hid.find(string("SID")) != string::npos) {
			// Don't calculate routes for SIDs
			continue;
		}
		rc = sendSidRoutingTable(it_router->second.hid, ADSIDsTable);
	}
	return rc;
}

int Controller::sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
	syslog(LOG_INFO, "Controller::Send-Table");

	// send routing table to router:destHID. ADSIDsTable is a remapping of SIDADsTable for easier use
	if (destHID == _myHID)
	{
		// If destHID is self, process immediately
		//syslog(LOG_INFO, "set local sid routes");
		return processSidRoutingTable(ADSIDsTable);

	} else {
		// If destHID is not SID, send to relevant router
		//
		Xroute::XrouteMsg msg;



		msg.set_type()
		ControlMessage msg(CTL_SID_ROUTING_TABLE, _myAD, _myHID);

		// for checking and resend
		msg.append(_myAD);
		msg.append(destHID);
		msg.append(_sid_ctl_seq);

		//Format: #2 AD1:[ #2 SID1(percentage 30),SID2(percentage 100)], AD2[ #1 SID1(percentage 70)]

		msg.append(ADSIDsTable.size());
		std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
		for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
		{
			msg.append(it_ad->second.size());
			msg.append(it_ad->first);
			std::map<std::string, ServiceState>::iterator it_sid;
			for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
			{
				msg.append(it_sid->first);
				// TODO: put information from service_state inside
				msg.append(it_sid->second.percentage);
			}
		}

		return msg.send(_source_sock, &_ddag);
	}
}

int Controller::processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
	int rc = 1;

	std::map<std::string, XIARouteEntry> xrt;
	_xr.getRoutes("AD", xrt);

	//change vector to map AD:RouteEntry for faster lookup
	std::map<std::string, XIARouteEntry> ADlookup;
	map<std::string, XIARouteEntry>::iterator ir;
	for (ir = xrt.begin(); ir != xrt.end(); ++ir) {
		ADlookup[ir->first] = ir->second;
	}

	std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
	for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
	{
		XIARouteEntry entry = ADlookup[it_ad->first];
		//syslog(LOG_INFO, "AD entry: %s, %d, %s, %lu", entry.xid.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
		std::map<std::string, ServiceState>::iterator it_sid;
		for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
		{
			// TODO: how to use flags to load balance? or add new fields into routing table for this propose
			// syslog(LOG_INFO, "add route %s, %hu, %s, %lu", it_sid->first.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
			//if (it_sid->second.percentage <= 0)
			//    syslog(LOG_DEBUG, "Removing routing entry: %s@%s", it_sid->first.c_str(), entry.xid.c_str());

			if (entry.xid == _myAD)
			{
				_xr.seletiveSetRoute(it_sid->first, -2, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first); //local AD, FIXME: why is port unsigned short? port could be negative numbers!
			} else {
				_xr.seletiveSetRoute(it_sid->first, entry.port, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first);
			}

		}
	}

	return rc;
}

int Controller::updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state)
{
	int rc = 1;
	//TODO: only record ones with newer version
	//TODO: only update publicly useful attributes
	//TODO: an entry is not there maybe because it is deleted,we should avoid that an older discovery message adds it back again
	if ( _SIDADsTable.count(SID) > 0 )
	{
		if (service_state.seq == 0) {
			syslog(LOG_DEBUG, "recording bad SID seq, abort");
			return -1;
		}
		if (_SIDADsTable[SID].count(AD) > 0 )
		{
			//update if version is newer
			if (_SIDADsTable[SID][AD].seq < service_state.seq || _SIDADsTable[SID][AD].seq - service_state.seq > SEQNUM_WINDOW)
			{
				//syslog(LOG_DEBUG, "Got %d > %d new discovery msg %s@%s, priority%d",service_state.seq, _SIDADsTable[SID][AD].seq, SID.c_str(), AD.c_str(), service_state.priority);
				if (_SIDADsTable[SID][AD].capacity != service_state.capacity) {
					_send_sid_decision = true; // update decision
				}
				_SIDADsTable[SID][AD].capacity = service_state.capacity;
				_SIDADsTable[SID][AD].capacity_factor = service_state.capacity_factor;
				_SIDADsTable[SID][AD].link_factor = service_state.link_factor;
				_SIDADsTable[SID][AD].priority = service_state.priority;
				_SIDADsTable[SID][AD].seq = service_state.seq;

			//TODO: update other parameters
			} else {
				//syslog(LOG_DEBUG, "Got %d < %d old discovery msg %s@%s, priority%d",service_state.seq, _SIDADsTable[SID][AD].seq, SID.c_str(), AD.c_str(), service_state.priority);
			}

		} else {
			// insert new entry
			_SIDADsTable[SID][AD] = service_state;
		}
	} else {
		// no sid record
		std::map<std::string, ServiceState> new_map;
		new_map[AD] = service_state;
		_SIDADsTable[SID] = new_map;
	}
	// TODO: make return value meaningful or make this function return void
	//syslog(LOG_INFO, "update SID:%s, capacity %d, f1 %d, f2 %d, priority %d, from %s", SID.c_str(), service_state.capacity, service_state.capacity_factor, service_state.link_factor, service_state.priority, AD.c_str());
	return rc;
}
#endif

void* Controller::updatePathThread(void* updating)
{
	UNUSED(updating);
#if 0
	// do the xping in a thread
	// updating is a bool* from the creator of this thread that indicates whether a new update thread is necessary
	// We do not want multiple update threads waiting if the updating is slower than the period the timer calls.
	// Hence, lock is not the approach we want
	// FIXME: using "xping 'RE ADx' " is wrong, we should get the latency to
	// the involved hosts in that AD, not the closest host from that AD.
	// FIXME: pinging the ADs one by one takes time, should make it parallel (multi-threaded)

	*((bool*) updating) = true; // do not reenter
	std::map<std::string, NodeStateEntry>::iterator it_ad;

	for (it_ad = _ADNetworkTable_temp.begin(); it_ad != _ADNetworkTable_temp.end(); ++it_ad)
	{
		if (it_ad->first == _myAD) {
			continue;
		}
		if (it_ad->first.length() < 3) { // abnormal entry, skip to avoid weird stuff
			continue;
		}

		//yslog(LOG_DEBUG, "send ping to %s", it_ad->first.c_str());
		FILE *in;
		char buff[BUF_SIZE];
		char cmd[BUF_SIZE] = "";
		char root[BUF_SIZE];
		int latency = -1;

		// send 5 pings (-c), interval 0.1s (-i), each timeout 1s (-t)
		// XSOCKCONF_SECTION makes sure xping is performed on the right host
		// Here we use -f 1 to find the minimal latency instead of average as ping delay is no stable in our current
		// implementation. Even small traffic like control messages can change the delay dramatically.
		sprintf(cmd,
			"XSOCKCONF_SECTION=%s %s/bin/xping -t 10 -q -c 5 'RE %s' | tail -1 | awk '{print $5}' | cut -d '/' -f 1",
			_hostname,
			XrootDir(root, BUF_SIZE),
			it_ad->first.c_str());
		if(!(in = popen(cmd, "r"))) {
			syslog(LOG_DEBUG, "Fail to execute %s", cmd);
			continue;
		}
		while(fgets(buff, sizeof(buff), in) != NULL) {
			if (strlen(buff) > 0) { // avoid bad things
				//syslog(LOG_DEBUG, "Ping %s result is %s",it_ad->first.c_str(), buff);
				latency = atoi(buff); // FIXME: atoi cannot detect error
			}
		}
		if (pclose(in) > 0) {
			syslog(LOG_DEBUG, "ping to %s timeout", it_ad->first.c_str());
		}
		if (latency >= 0) {
			//syslog(LOG_DEBUG, "latency to %s is %d", it_ad->first.c_str(), latency);
			ADPathState ADpath_state;
			ADpath_state.delay = latency>0?latency:9999; // maybe a timeout/atoi failure
			int delta = _ADPathStates[it_ad->first].delay - ADpath_state.delay;
			if ( delta > 5 || delta < -5 ) {
				_send_sid_decision = true; // re-query the decision because latency is changed
				//TODO: only query that SID.
			}
			//This is probably thread-safe TODO: lock
			_ADPathStates[it_ad->first] = ADpath_state;
		}
	}
	*((bool*) updating) = false; // enter
#endif
    pthread_exit(NULL);
}

int Controller::updateADPathStates(void)
{   // udpate the states of paths (latency) to each AD
	// FIXME: routers could have different latencies to the same AD, this
	// process should be done in each router
	//syslog(LOG_DEBUG, "updateADPathStates called , try to send to %lu ADs", _ADNetworkTable.size());
	int rc = 1;
	//Don't make the update when the previous one is ongoing
	static bool updating = false;

	// Create a new thread so that the daemon could receive control messages while waiting for xping
	pthread_t newthread;
	if (!updating) {
		// copy to local, avoid data hazard
		_ADNetworkTable_temp = _ADNetworkTable;
		if (pthread_create(&newthread , NULL, updatePathThread, &updating)!= 0) {
			perror("pthread_create");
		} else {
			pthread_detach(newthread); // leave the thread along
		}
	}


	// latency to itself is always 1ms (minimal value)
	// FIXME: above is not true!
	ADPathState ADpath_state;
	ADpath_state.delay = 1;
	_ADPathStates[_myAD] = ADpath_state;
	return rc;
}
#if 0
int Controller::sendSidDiscovery()
{
	int rc = 1;

	syslog(LOG_INFO, "Controller::Send-SID Disc");

	Node ad(_myAD);
	Node hid(_myHID);

	Xroute::XrouteMsg msg;
	Xroute::SIDDiscoveryMsg *d = msg.mutable_sid_discovery();
	Xroute::XID             *a = d  ->mutable_ad();
	Xroute::XID             *h = d  ->mutable_hid();

	a->set_type(ad.type());
	a->set_id(ad.id(), XID_SIZE);
	h->set_type(hid.type());		// FIXME: original code used the ad over again
	h->set_id(hid.id(), XID_SIZE);	//  and once again doesn't use the hid field at all

	//broadcast local services
	if (_LocalSidList.empty() && _SIDADsTable.empty())
	{
		// Nothing to send
		// syslog(LOG_INFO, "%s LocalSidList is empty, nothing to send", _myAD);
		return rc;

	} else {
		// prepare the packet TODO: only send updated entries TODO: but don't forget to renew TTL
		sendKeepAliveToServiceControllerLeader(); // temporally put it here
		// TODO: the format of this packet could be reduced much
		// local info
		std::map<std::string, ServiceState>::iterator it;
		for (it = _LocalSidList.begin(); it != _LocalSidList.end(); ++it)
		{
			Node s(it->first);

			Xroute::DiscoveryEntry *e = d->add_entries();

			Xroute::XID *x = e->mutable_ad();
			x->set_type(ad.type());
			x->set_id(ad.id(), XID_SIZE);

			x = e->mutable_sid();
			x->set_type(s.type());
			x->set_id(s.id(), XID_SIZE);

			// TODO: append more attributes/parameters
			e->set_capacity(it->second.capacity);
			e->set_capacity_factor(it->second.capacity_factor);
			e->set_link_factor(it->second.link_factor);
			e->set_priority(it->second.priority);
			e->set_leader_addr(it->second.leaderAddr);
			e->set_arch_type(it->second.archType);
			e->set_seq(it->second.seq);
		}

		// rebroadcast services learnt from others
		std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
		for (it_sid = _SIDADsTable.begin(); it_sid != _SIDADsTable.end(); ++it_sid)
		{
			std::map<std::string, ServiceState>::iterator it_ad;
			for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
			{
				Node aa(it_ad->first);
				Node ss(it_sid->first);

				Xroute::DiscoveryEntry *e = d->add_entries();

				Xroute::XID *x = e->mutable_ad();
				x->set_type(aa.type());
				x->set_id(aa.id(), XID_SIZE);

				x = e->mutable_sid();
				x->set_type(ss.type());
				x->set_id(ss.id(), XID_SIZE);

				// TODO: put information from service_state inside
				e->set_capacity(it_ad->second.capacity);
				e->set_capacity_factor(it_ad->second.capacity_factor);
				e->set_link_factor(it_ad->second.link_factor);
				e->set_priority(it_ad->second.priority);
				e->set_leader_addr(it_ad->second.leaderAddr);
				e->set_arch_type(it_ad->second.archType);
				e->set_seq(it_ad->second.seq);
			}
		}
	}

	// send it to neighbor ADs
	// TODO: reuse code: broadcastToADNeighbors(msg)?

	// locallly process first
	processSidDiscovery(msg.sid_discovery());

	std::map<std::string, NeighborEntry>::iterator it;

	for (it = _ADNeighborTable.begin(); it != _ADNeighborTable.end(); ++it)
	{
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(_controller_sid);
		g.fill_sockaddr(&ddag);

		string message;
		msg.	SerializeToString(&message);

		//syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", _lsa_seq, it->second.AD.c_str());
		//syslog(LOG_INFO, "msg: %s", msg.c_str());
		int temprc = Xsendto(_csock, message.c_str(), message.length(), 0, (sockaddr*)&_ddag, sizeof(sockaddr_x));
		if (temprc < 0) {
			syslog(LOG_ERR, "error sending inter-AD SID discovery to %s", it->second.AD.c_str());
		}
		rc = (temprc < rc)? temprc : rc;
	}

	return rc;
}
#endif

int Controller::sendKeepAliveToServiceControllerLeader()
{
	int rc = 1;

	std::map<std::string, ServiceState>::iterator it;
	for (it = _LocalSidList.begin(); it != _LocalSidList.end(); ++it)
	{
		Node sid(it->first);
		Node  ad(_myAD);
		Node hid(_myHID);

		Xroute::XrouteMsg msg;
		Xroute::SidKeepAliveMsg *ka = msg.mutable_keep_alive();
		Xroute::Node *n  = ka->mutable_from();
		Xroute::XID  *a  = n ->mutable_ad();
		Xroute::XID  *h  = n ->mutable_hid();
		Xroute::XID  *s  = ka->mutable_sid();

		msg.set_type(Xroute::SID_MANAGE_KA_MSG);
		msg.set_version(Xroute::XROUTE_PROTO_VERSION);
		a->set_type(ad.type());
		a->set_id(ad.id(), XID_SIZE);
		h->set_type(hid.type());
		h->set_id(hid.id(), XID_SIZE);
		s->set_type(sid.type());
		s->set_id(sid.id(), XID_SIZE);
		ka->set_capacity(it->second.capacity);
		ka->set_delay(it->second.internal_delay);

		// FIXME: there is a lot more in the servicestate struct than is being used here

		if (it->second.isLeader) {
			processServiceKeepAlive(msg.sid_keep_alive()); // process locally

		} else {
			sockaddr_x ddag;
			xia_pton(AF_XIA, it->second.leaderAddr.c_str(), &ddag);

			int temprc = sendMessage(&ddag, msg);
			rc = (temprc < rc)? temprc : rc;
			//syslog(LOG_DEBUG, "sent SID %s keep alive to %s", it->first.c_str(), it->second.leaderAddr.c_str());
		}
	}

	return rc;
}

int Controller::processServiceKeepAlive(const Xroute::SidKeepAliveMsg &msg)
{
	int rc = 1;
	string neighborAD, neighborHID, sid;

	Xroute::XID xad  = msg.from().ad();
	Xroute::XID xhid = msg.from().hid();
	Xroute::XID xsid = msg.sid();

	neighborAD  = Node(xad.type(),  xad.id().c_str(),  0).to_string();
	neighborHID = Node(xhid.type(), xhid.id().c_str(), 0).to_string();
	sid         = Node(xsid.type(), xsid.id().c_str(), 0).to_string();

	uint32_t capacity = msg.capacity();
	uint32_t delay    = msg.delay();


	if (_LocalServiceLeaders.find(sid) != _LocalServiceLeaders.end()) {
		// if controller is here
		//update attributes
		_LocalServiceLeaders[sid].instances[neighborAD].capacity = capacity;
		_LocalServiceLeaders[sid].instances[neighborAD].internal_delay = delay;
		// TODO: reply to that instance?
		//syslog(LOG_DEBUG, "got SID %s keep alive from %s", sid.c_str(), srcAddr.c_str());

	} else {
		syslog(LOG_ERR, "got %s keep alive to its service leader from %s, but I am not its leader!", sid.c_str(), neighborAD.c_str());
		rc = -1;
	}

	return rc;
}
void Controller::set_sid_conf(const char* myhostname)
{
	UNUSED(myhostname);
#if 0
	// read the controller file at etc/controller$i_sid.ini
	// the file defines which AD has what SIDs
	// ini style file
	// the file could be loaded frequently to emulate dynamic service changes

	// time to have some update of the local services
	_sid_discovery_seq = (_sid_discovery_seq + 1) % MAX_SEQNUM;

	char full_path[BUF_SIZE];
	char root[BUF_SIZE];
	char section_name[BUF_SIZE];
	char controller_addr[BUF_SIZE];
	char sid[BUF_SIZE];
	std::string service_sid;

	int section_index = 0;
	snprintf(full_path, BUF_SIZE, "%s/etc/%s_sid.ini", XrootDir(root, BUF_SIZE), myhostname);
	std::map<std::string, ServiceState> old_local_list = _LocalSidList; // to check if some entries is being removed. NOTE: may need deep copy in the future or more efficient way to do this
	_LocalSidList.clear(); // clean and reload all

	while (ini_getsection(section_index, section_name, BUF_SIZE, full_path)) //enumerate every section, [$name]
	{
		//fprintf(stderr, "read %s\n", section_name);
		ini_gets(section_name, "sid", sid, sid, BUF_SIZE, full_path);
		service_sid = std::string(sid);

		/*first read the synthetic load as a client domain*/
		int s_load = ini_getl(section_name, "synthetic_load", 0, full_path);
		if (s_load == 0) { // synthetic load is not set, make sure it won't overwrite the existing value
			if (_SIDRateTable.find(service_sid) == _SIDRateTable.end()) {
				_SIDRateTable[service_sid] = 0;
			} else {
				// do nothing
			}
		} else {
			_SIDRateTable[service_sid] = s_load;
		}


		if (ini_getbool(section_name, "enabled", 0, full_path) == 0)
		{// skip this if not enabled, NOTE: enabled=True is the default one
		 // if an entry was enabled but is disabled now, we should 'poison' other ADs by broadcasting this invalidation
			if (old_local_list.count(service_sid) > 0) { // it was there
				old_local_list[service_sid].priority = -1; // invalid entry
				old_local_list[service_sid].seq = _sid_discovery_seq;
				_LocalSidList[service_sid] = old_local_list[service_sid]; // NOTE: may need deep copy someday
				//syslog(LOG_DEBUG, "Poisoning %s\n", sid);
			}
			//else just ignore it
		} else {
			ServiceState service_state;
			service_state.seq = _sid_discovery_seq;
			service_state.capacity = ini_getl(section_name, "capacity", 100, full_path);
			service_state.capacity_factor = ini_getl(section_name, "capacity_factor", 1, full_path);
			service_state.link_factor = ini_getl(section_name, "link_factor", 0, full_path);
			service_state.priority = ini_getl(section_name, "priority", 1, full_path);
			service_state.isLeader = ini_getbool(section_name, "isleader", 0, full_path);
			// get the address of the service controller
			ini_gets(section_name, "leaderaddr", controller_addr, controller_addr, BUF_SIZE, full_path);
			service_state.leaderAddr = std::string(controller_addr);
			if (service_state.isLeader) // I am the leader
			{
				if (_LocalServiceLeaders.find(service_sid) == _LocalServiceLeaders.end())
				{ // no service controller yet, create one
					ServiceLeader sc;
					_LocalServiceLeaders[service_sid] = sc; // just initialize it
				}
			}
			service_state.archType = ini_getl(section_name, "archType", 0, full_path);
			service_state.priority = ini_getl(section_name, "priority", 1, full_path);
			service_state.internal_delay = ini_getl(section_name, "internaldelay", 0, full_path);
			int decision_type = ini_getl(section_name, "decisiontype", 0, full_path);
			switch (decision_type)
			{
				case LATENCY_FIRST:
					service_state.decision = &Controller::Latency_first;
					break;
				case PURE_LOADBALANCE:
					service_state.decision = &Controller::Load_balance;
					break;
				case RATE_LOADBALANCE:
					service_state.decision = &Controller::Rate_load_balance;
					break;
				default:
					syslog(LOG_DEBUG, "unknown decision function %s\n", sid);
			}

			//fprintf(stderr, "read state%s, %d, %d, %s\n", sid, service_state.capacity, service_state.isLeader, service_state.leaderAddr.c_str());
			if (_LocalSidList[service_sid].capacity != service_state.capacity) {
				_send_sid_discovery = true; //update
			}
			_LocalSidList[service_sid] = service_state;
		}

		section_index++;
	}

	return;
#endif
}

