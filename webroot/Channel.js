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
  }


  membersNames()
  {
    return this.members.map(member => member.name);
  }


  formatMembers()
  {
    let names = this.membersNames();
    return names.length > 0 ? names.join(", ") : `Channel ${this.channelId}`;
  }
}
