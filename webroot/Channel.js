class Channel
{
  constructor(channelData, node = null)
  {
    this.channelId = channelData.channelId;
    this.members = channelData.devices;
    this.gammaFactor = channelData.gammaFactor;
    this.node = node;

    this.uvs = null;
    if(channelData.uvs){
      this.uvs = channelData.uvs;
    }

    // Index of the captured monitor the region is on, null if that monitor isn't captured
    this.monitor = channelData.monitor ?? null;
    this.monitorMissing = channelData.monitorMissing || false;

    // Color of the surface the lamp shines on, null for the common wall
    this.wallColor = channelData.wallColor ?? null;
  }


  membersNames()
  {
    return this.members.map(member => member.name);
  }


  formatMembers()
  {
    let names = this.membersNames();
    return names.length > 0 ? names.join(", ") : t("Channel {id}", {id : this.channelId});
  }
}
