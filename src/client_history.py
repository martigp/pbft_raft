class ClientInformation:
    """In memory store of client information.

    Importantly includes the latest request Id's corresponding seen in
    each stage of chained hotstuff.
    """

    def __init__(self, clientPk : str, latestReqId : int = -1, latestProposedId : int = -1,
                 latestLockedId : int = -1, latestCommitedId : int = -1):
        self.clientPk : str = clientPk
        # Latest req_id of of client
        self.latestReqId : int = latestReqId
        # Latest req_id for a proposal for this client
        self.latestProposedId : int = latestProposedId
        # Latest req_id that is locked for this client
        self.latestLockedId : int = latestLockedId
        # Latest req_id that is commited / executed for this client
        self.latestCommitedId : int = latestCommitedId
    
    def updateReq(self, reqId : int):
        if reqId > self.latestReqId:
            self.latestReqId = reqId
    
    def updateProposed(self, reqId : int):
        if reqId > self.latestProposedId:
            self.latestProposedId = reqId
    
    def updateLocked(self, reqId : int):
        if reqId > self.latestLockedId:
            self.latestLockedId = reqId
    
    def updateCommited(self, reqId : int):
        if reqId > self.latestCommitedId:
            self.latestCommitedId = reqId
