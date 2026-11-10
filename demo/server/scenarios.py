"""Scenario definitions for the PSI demo."""

SCENARIOS = {
    "ad": {
        "id": "ad",
        "name": "广告转化归因",
        "icon": "AD",
        "tagline": "广告平台有曝光名单、商家有成交订单、支付机构有支付流水；三方只求共同用户数，不交换原始名单。",
        "party_a": "广告平台",
        "party_b": "商家",
        "party_c": "支付机构",
        "data_a": "曝光名单",
        "data_b": "成交订单",
        "data_c": "支付流水",
        "element": "脱敏用户 ID",
        "element_format": "uid_ + 6 位十六进制",
        "element_example": "uid_4f9a2c",
        "threshold_label": "最少归因成交人数 k",
        "action_accept": "三方共同用户数达到 k：输出归因成交人数，可按人数结算或优化投放；原始名单仍不出域。",
        "action_reject": "三方共同用户数不足 k：不输出人级明细，只返回“本次归因不达标”。",
        "example": {"size_a": 1048576, "size_b": 32768, "size_c": 30000, "intersection": 12345, "threshold": 8000},
        "real_note": "三方交集 = 既曝光、又成交、又完成支付的脱敏用户数。",
        "default_n_log": 20,
        "default_threshold": 8000,
        "default_common": 12345,
        "security": "半诚实 / 恶意（论文两档）",
        "note": "论文 1 的广告转化率场景；百万级数据按论文口径展示。",
    },
    "ride": {
        "id": "ride",
        "name": "拼车路线匹配",
        "icon": "CAR",
        "tagline": "司机有轨迹网格、乘客有路线网格、调度有路网索引；三方只比较共同路段网格数，不暴露精确轨迹。",
        "party_a": "司机",
        "party_b": "乘客",
        "party_c": "调度",
        "data_a": "司机轨迹网格",
        "data_b": "乘客路线网格",
        "data_c": "路网索引",
        "element": "地理网格 / 虚拟点",
        "element_format": "geo_<纬度>_<经度>，约 100 米网格",
        "element_example": "geo_31.230_121.470",
        "threshold_label": "最少共同路段网格 k",
        "action_accept": "共同路段网格达到 k：进入拼车定价/接驾；双方不暴露完整轨迹。",
        "action_reject": "共同路段网格不足 k：不匹配，不返回具体重合路段。",
        "example": {"size_a": 16384, "size_b": 16384, "size_c": 12288, "intersection": 9800, "threshold": 9000},
        "real_note": "三方交集 = 司机、乘客、调度都覆盖到的路段网格。",
        "default_n_log": 14,
        "default_threshold": 9000,
        "default_common": 9800,
        "security": "恶意模型（论文 4）",
        "note": "论文 4 的 IoV 拼车场景；当前后端实测规模建议不超过 2^17。",
    },
    "gene": {
        "id": "gene",
        "name": "基因医疗比对",
        "icon": "DNA",
        "tagline": "患者有脱敏位点、医院有参考库、科研库有标注位点；三方只判断共同位点是否达到科研/辅助诊断门槛，不交换全量基因数据。",
        "party_a": "患者",
        "party_b": "医院",
        "party_c": "科研库",
        "data_a": "脱敏位点",
        "data_b": "医院参考库",
        "data_c": "科研标注位点",
        "element": "脱敏位点 ID",
        "element_format": "locus_rs + 6 位数字",
        "element_example": "locus_rs123456",
        "threshold_label": "最少共同位点数 k",
        "action_accept": "共同位点达到 k：进入辅助诊断/科研复核；只返回脱敏位点 ID，不返回全量基因数据。",
        "action_reject": "共同位点不足 k：不输出位点明细，只返回未达门槛。",
        "example": {"size_a": 65536, "size_b": 65536, "size_c": 12000, "intersection": 7200, "threshold": 6000},
        "real_note": "仅合成数据演示，非真实患者；三方交集 = 患者、医院参考库、科研库共同标注的脱敏位点。",
        "default_n_log": 16,
        "default_threshold": 6000,
        "default_common": 7200,
        "security": "半诚实（演示口径）",
        "note": "医院/基因场景仅做合成数据演示，不使用真实患者数据。",
    },
}

DISABLED_SCENARIOS = [
    {
        "id": "edu",
        "name": "儿童教育（合规评估中）",
        "icon": "EDU",
        "tagline": "涉及未成年人数据与效果承诺，先做合法性/合理性评估；本次 Demo 不使用真实数据。",
        "reason": "需法务与合规评审，不在本次两周交付范围。",
    }
]


def get_scenario(scenario_id: str) -> dict:
    if scenario_id not in SCENARIOS:
        raise ValueError(f"unknown scenario: {scenario_id}")
    return SCENARIOS[scenario_id]


def default_config(scenario_id: str) -> dict:
    scenario = get_scenario(scenario_id)
    return {
        "scenario": scenario_id,
        "n_log": scenario["default_n_log"],
        "threshold": scenario["default_threshold"],
        "common": scenario["default_common"],
        "parties": 2,
        "mode": "mock",
        "security": "semi-honest",
    }
