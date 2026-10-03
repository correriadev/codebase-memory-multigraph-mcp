# Retention probe h_retention_restart_20261002_160437_16824

## Scope
Isolated HarnessKit graph snapshot; owner exit and daemon restart.

## Artifact
Artifact ID: ART-RETENTION-2026100216043716824
Expected SHA-256: 28a4ea7ac6bee3d9e6647cd5677c044547ee0992216986d0332a0612780a5b42
Exact text: retention_probe|owner_exit|daemon_restart|h_retention_restart_20261002_160437_16824

## Graph
```tactical-spec
{
  "nodes": [
    {
      "symbol": "AR-01",
      "type": "FractalArtifact",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_160437_16824/retention-probe.md#AR-01",
      "description": "{\"id\":\"AR-01\",\"artifact_id\":\"ART-RETENTION-2026100216043716824\",\"temenos_id\":\"h_retention_restart_20261002_160437_16824\",\"part_count\":2,\"part_ids\":[\"ART-RETENTION-2026100216043716824-P01\",\"ART-RETENTION-2026100216043716824-P02\"],\"join_policy\":\"direct\",\"byte_count\":83,\"sha256\":\"28a4ea7ac6bee3d9e6647cd5677c044547ee0992216986d0332a0612780a5b42\"}"
    },
    {
      "symbol": "AP-01",
      "type": "FractalArtifactPart",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_160437_16824/retention-probe.md#AP-01",
      "description": "{\"id\":\"AP-01\",\"artifact_id\":\"ART-RETENTION-2026100216043716824\",\"temenos_id\":\"h_retention_restart_20261002_160437_16824\",\"part_id\":\"ART-RETENTION-2026100216043716824-P01\",\"part_index\":1,\"total_parts\":2,\"exact_content\":\"retention_probe|owner_exit|daemon_restart\",\"byte_count\":41,\"sha256\":\"5cf584fc7934436d0c2ac78e7019b13e21a7324bf9e5d42231bbb84ce6d81d0b\"}"
    },
    {
      "symbol": "AP-02",
      "type": "FractalArtifactPart",
      "cbm_uri": "cbm://C-Users-corre-Documents-harness-kit/docs/temenos/h_retention_restart_20261002_160437_16824/retention-probe.md#AP-02",
      "description": "{\"id\":\"AP-02\",\"artifact_id\":\"ART-RETENTION-2026100216043716824\",\"temenos_id\":\"h_retention_restart_20261002_160437_16824\",\"part_id\":\"ART-RETENTION-2026100216043716824-P02\",\"part_index\":2,\"total_parts\":2,\"exact_content\":\"|h_retention_restart_20261002_160437_16824\",\"byte_count\":42,\"sha256\":\"95b4f92ba1dd3120af7e64a568fd04d6ede6ca2f065f4f948a3a6a2d32fc904a\"}"
    }
  ],
  "edges": [
    {
      "source": "AR-01",
      "target": "AP-01",
      "type": "CONTAINS_PART"
    },
    {
      "source": "AP-01",
      "target": "AP-02",
      "type": "NEXT_PART"
    },
    {
      "source": "AR-01",
      "target": "AP-02",
      "type": "CONTAINS_PART"
    }
  ]
}
```
