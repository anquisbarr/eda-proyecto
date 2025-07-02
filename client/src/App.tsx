import { useRef, useState, useEffect } from "react";
import "./App.css";

interface CompareRequest {
	query_index: number;
	k: number;
}

interface CompareResponse {
	job_id: string;
	status: string;
}

interface DistanceMatch {
	distance_squared: number;
	is_match: boolean;
}

interface AlgorithmResult {
	algorithm_name: string;
	precision: number;
	execution_time_ms: number;
	success: boolean;
	distances_and_matches: DistanceMatch[];
}

interface JobResult {
	job_id: string;
	k: number;
	completed: boolean;
	query_info: string;
	total_time_ms: number;
	results: AlgorithmResult[];
}

interface QueryContext {
	max_dataset_index: number;
	num_query_vectors: number;
	dataset_size: number;
	dataset_info: string;
	query_info: string;
}

function App() {
	const [queryIndex, setQueryIndex] = useState<number>(20);
	const [k, setK] = useState<number>(10);
	const [loading, setLoading] = useState<boolean>(false);
	const [jobId, setJobId] = useState<string>("");
	const [result, setResult] = useState<JobResult | null>(null);
	const [error, setError] = useState<string>("");
	const [viewMode, setViewMode] = useState<"cards" | "comparison" | "detailed">(
		"cards",
	);
	const [context, setContext] = useState<QueryContext | null>(null);
	const [contextLoading, setContextLoading] = useState<boolean>(false);
	const [contextError, setContextError] = useState<string>("");
	const pollingInterval = useRef<number | null>(null);

	const API_BASE = "http://localhost:8080/api";

	const submitCompareRequest = async () => {
		setLoading(true);
		setError("");
		setResult(null);

		try {
			const response = await fetch(`${API_BASE}/compare`, {
				method: "POST",
				headers: {
					"Content-Type": "application/json",
				},
				body: JSON.stringify({
					query_index: queryIndex,
					k: k,
				} as CompareRequest),
			});

			if (!response.ok) {
				throw new Error(`HTTP error! status: ${response.status}`);
			}

			const data: CompareResponse = await response.json();
			setJobId(data.job_id);

			// Start polling for results
			startPolling(data.job_id);
		} catch (err) {
			setError(
				`Failed to submit request: ${err instanceof Error ? err.message : "Unknown error"}`,
			);
			setLoading(false);
		}
	};

	const startPolling = (id: string) => {
		pollingInterval.current = setInterval(async () => {
			try {
				const response = await fetch(`${API_BASE}/result?job_id=${id}`);

				if (!response.ok) {
					throw new Error(`HTTP error! status: ${response.status}`);
				}

				const data: JobResult = await response.json();

				if (data.completed) {
					setResult(data);
					setLoading(false);
					if (pollingInterval.current) {
						clearInterval(pollingInterval.current);
						pollingInterval.current = null;
					}
				}
			} catch (err) {
				setError(
					`Failed to fetch results: ${err instanceof Error ? err.message : "Unknown error"}`,
				);
				setLoading(false);
				if (pollingInterval.current) {
					clearInterval(pollingInterval.current);
					pollingInterval.current = null;
				}
			}
		}, 1000); // Poll every second
	};

	const resetForm = () => {
		setJobId("");
		setResult(null);
		setError("");
		setLoading(false);
		if (pollingInterval.current) {
			clearInterval(pollingInterval.current);
			pollingInterval.current = null;
		}
	};

	// Load context when component mounts
	useEffect(() => {
		const loadContext = async () => {
			setContextLoading(true);
			setContextError("");

			try {
				const response = await fetch(`${API_BASE}/context`);

				if (!response.ok) {
					throw new Error(`HTTP error! status: ${response.status}`);
				}

				const data: QueryContext = await response.json();
				setContext(data);
			} catch (err) {
				setContextError(
					`Failed to fetch context: ${err instanceof Error ? err.message : "Unknown error"}`,
				);
			} finally {
				setContextLoading(false);
			}
		};

		loadContext();
	}, []);

	// Update query index when context loads to ensure it's within bounds
	useEffect(() => {
		if (context && queryIndex > context.max_dataset_index) {
			setQueryIndex(Math.min(context.max_dataset_index, 20));
		}
	}, [context, queryIndex]);

	// Validate query index bounds
	const isQueryIndexValid = () => {
		if (!context) return true; // Allow if context not loaded yet
		return queryIndex >= 0 && queryIndex <= context.max_dataset_index;
	};

	const getQueryIndexError = () => {
		if (!context) return "";
		if (queryIndex < 0) return "Query index must be non-negative";
		if (queryIndex > context.max_dataset_index) {
			return `Query index must be ≤ ${context.max_dataset_index}`;
		}
		return "";
	};

	const generateRandomQuery = () => {
		if (!context) return;
		const randomIndex = Math.floor(
			Math.random() * (context.max_dataset_index + 1),
		);
		setQueryIndex(randomIndex);
	};

	const refreshContext = () => {
		setContext(null);
		setContextError("");

		const loadContext = async () => {
			setContextLoading(true);
			setContextError("");

			try {
				const response = await fetch(`${API_BASE}/context`);

				if (!response.ok) {
					throw new Error(`HTTP error! status: ${response.status}`);
				}

				const data: QueryContext = await response.json();
				setContext(data);
			} catch (err) {
				setContextError(
					`Failed to fetch context: ${err instanceof Error ? err.message : "Unknown error"}`,
				);
			} finally {
				setContextLoading(false);
			}
		};

		loadContext();
	};

	// Helper functions for analysis
	const getPerformanceRanking = () => {
		if (!result) return [];
		return [...result.results]
			.sort((a, b) => a.execution_time_ms - b.execution_time_ms)
			.map((alg, index) => ({
				...alg,
				rank: index + 1,
				speedRank: index + 1,
			}));
	};

	const getBestAlgorithm = () => {
		if (!result) return null;
		const successful = result.results.filter((r) => r.success);
		if (successful.length === 0) return null;

		// Sort by precision (desc) then by speed (asc)
		return successful.sort((a, b) => {
			if (Math.abs(a.precision - b.precision) < 0.0001) {
				return a.execution_time_ms - b.execution_time_ms;
			}
			return b.precision - a.precision;
		})[0];
	};

	const getWorstAlgorithm = () => {
		if (!result) return null;
		const successful = result.results.filter((r) => r.success);
		if (successful.length === 0) return null;

		return successful.sort((a, b) => {
			if (Math.abs(a.precision - b.precision) < 0.0001) {
				return b.execution_time_ms - a.execution_time_ms;
			}
			return a.precision - b.precision;
		})[0];
	};

	const getFastestAlgorithm = () => {
		if (!result) return null;
		const successful = result.results.filter((r) => r.success);
		return successful.length > 0
			? successful.sort((a, b) => a.execution_time_ms - b.execution_time_ms)[0]
			: null;
	};

	const getAccuracyLeader = () => {
		if (!result) return null;
		const successful = result.results.filter((r) => r.success);
		return successful.length > 0
			? successful.sort((a, b) => b.precision - a.precision)[0]
			: null;
	};

	const renderPerformanceInsights = () => {
		if (!result) return null;

		const best = getBestAlgorithm();
		const fastest = getFastestAlgorithm();
		const mostAccurate = getAccuracyLeader();
		const successfulAlgorithms = result.results.filter((r) => r.success).length;

		return (
			<div className="insights-section">
				<h3>📊 Performance Insights</h3>
				<div className="insights-grid">
					<div className="insight-card highlight">
						<div className="insight-icon">🏆</div>
						<div className="insight-content">
							<h4>Best Overall</h4>
							<p>{best?.algorithm_name || "N/A"}</p>
							<small>
								{best
									? `${(best.precision * 100).toFixed(1)}% precision, ${best.execution_time_ms.toFixed(2)}ms`
									: ""}
							</small>
						</div>
					</div>
					<div className="insight-card">
						<div className="insight-icon">⚡</div>
						<div className="insight-content">
							<h4>Fastest</h4>
							<p>{fastest?.algorithm_name || "N/A"}</p>
							<small>
								{fastest ? `${fastest.execution_time_ms.toFixed(2)}ms` : ""}
							</small>
						</div>
					</div>
					<div className="insight-card">
						<div className="insight-icon">🎯</div>
						<div className="insight-content">
							<h4>Most Accurate</h4>
							<p>{mostAccurate?.algorithm_name || "N/A"}</p>
							<small>
								{mostAccurate
									? `${(mostAccurate.precision * 100).toFixed(1)}% precision`
									: ""}
							</small>
						</div>
					</div>
					<div className="insight-card">
						<div className="insight-icon">✅</div>
						<div className="insight-content">
							<h4>Success Rate</h4>
							<p>
								{successfulAlgorithms}/{result.results.length}
							</p>
							<small>
								{((successfulAlgorithms / result.results.length) * 100).toFixed(
									0,
								)}
								% successful
							</small>
						</div>
					</div>
				</div>
			</div>
		);
	};

	const renderComparisonTable = () => {
		if (!result) return null;

		const ranking = getPerformanceRanking();

		return (
			<div className="comparison-table-container">
				<h3>📈 Algorithm Comparison</h3>
				<div className="table-wrapper">
					<table className="comparison-table">
						<thead>
							<tr>
								<th>Rank</th>
								<th>Algorithm</th>
								<th>Precision</th>
								<th>Execution Time</th>
								<th>Status</th>
								<th>Performance Score</th>
							</tr>
						</thead>
						<tbody>
							{ranking.map((algorithm, index) => {
								const performanceScore = algorithm.success
									? algorithm.precision * 0.7 +
										(1 / algorithm.execution_time_ms) * 1000 * 0.3
									: 0;
								const isTop = index === 0 && algorithm.success;

								return (
									<tr
										key={algorithm.algorithm_name}
										className={isTop ? "top-performer" : ""}
									>
										<td>
											<span
												className={`rank-badge ${isTop ? "rank-gold" : index === 1 ? "rank-silver" : index === 2 ? "rank-bronze" : ""}`}
											>
												{isTop
													? "🥇"
													: index === 1
														? "🥈"
														: index === 2
															? "🥉"
															: `#${index + 1}`}
											</span>
										</td>
										<td className="algorithm-name">
											{algorithm.algorithm_name}
										</td>
										<td>
											<div className="precision-cell">
												<div className="precision-bar">
													<div
														className="precision-fill"
														style={{ width: `${algorithm.precision * 100}%` }}
													></div>
												</div>
												<span>{(algorithm.precision * 100).toFixed(2)}%</span>
											</div>
										</td>
										<td>
											<span className="time-value">
												{algorithm.execution_time_ms.toFixed(4)} ms
											</span>
										</td>
										<td>
											<span
												className={`status-badge ${algorithm.success ? "success" : "failed"}`}
											>
												{algorithm.success ? "✅ Success" : "❌ Failed"}
											</span>
										</td>
										<td>
											<div className="score-cell">
												<div className="score-bar">
													<div
														className="score-fill"
														style={{
															width: `${Math.min(performanceScore * 10, 100)}%`,
														}}
													></div>
												</div>
												<span>{performanceScore.toFixed(1)}</span>
											</div>
										</td>
									</tr>
								);
							})}
						</tbody>
					</table>
				</div>
			</div>
		);
	};

	const renderDetailedView = () => {
		if (!result) return null;

		return (
			<div className="detailed-view">
				<h3>🔍 Detailed Analysis</h3>
				{result.results.map((algorithm) => (
					<div
						key={algorithm.algorithm_name}
						className="detailed-algorithm-card"
					>
						<div className="algorithm-header">
							<h4>{algorithm.algorithm_name}</h4>
							<div className="algorithm-badges">
								<span
									className={`status-badge ${algorithm.success ? "success" : "failed"}`}
								>
									{algorithm.success ? "✅" : "❌"}
								</span>
								{algorithm.precision === 1.0 && (
									<span className="badge perfect">Perfect</span>
								)}
								{algorithm === getFastestAlgorithm() && (
									<span className="badge fastest">Fastest</span>
								)}
								{algorithm === getAccuracyLeader() && (
									<span className="badge accurate">Most Accurate</span>
								)}
							</div>
						</div>

						<div className="algorithm-metrics">
							<div className="metric">
								<span className="metric-label">Precision</span>
								<div className="metric-visual">
									<div className="circular-progress">
										<div
											className="circle"
											style={{
												background: `conic-gradient(#4ade80 ${algorithm.precision * 360}deg, #e5e7eb 0deg)`,
											}}
										>
											<span>{(algorithm.precision * 100).toFixed(1)}%</span>
										</div>
									</div>
								</div>
							</div>

							<div className="metric">
								<span className="metric-label">Execution Time</span>
								<div className="metric-value">
									<span className="time-large">
										{algorithm.execution_time_ms.toFixed(4)}
									</span>
									<span className="time-unit">ms</span>
								</div>
							</div>

							<div className="metric">
								<span className="metric-label">Matches Found</span>
								<div className="metric-value">
									<span className="matches-count">
										{
											algorithm.distances_and_matches.filter((m) => m.is_match)
												.length
										}
									</span>
									<span className="matches-total">
										/ {algorithm.distances_and_matches.length}
									</span>
								</div>
							</div>
						</div>

						<div className="distances-detailed">
							<h5>Distance Results</h5>
							<div className="distances-chart">
								{algorithm.distances_and_matches.map((match, index) => (
									<div
										key={`${algorithm.algorithm_name}-${index}`}
										className={`distance-bar ${match.is_match ? "match" : "no-match"}`}
									>
										<div className="distance-info">
											<span className="distance-rank">#{index + 1}</span>
											<span className="distance-value">
												{match.distance_squared.toFixed(0)}
											</span>
											<span
												className={`match-indicator ${match.is_match ? "match" : "no-match"}`}
											>
												{match.is_match ? "✓" : "✗"}
											</span>
										</div>
										<div className="distance-visual">
											<div
												className="distance-fill"
												style={{
													width: `${Math.min((match.distance_squared / 300000) * 100, 100)}%`,
												}}
											></div>
										</div>
									</div>
								))}
							</div>
						</div>
					</div>
				))}
			</div>
		);
	};

	return (
		<div className="app-container">
			<h1>Algorithm Comparison Dashboard</h1>

			<div className="main-content">
				<div className="form-sidebar">
					<div className="form-section">
						<h2>Query Parameters</h2>

						{/* Context Information */}
						{contextLoading && (
							<div className="context-info loading">
								<div className="spinner"></div>
								Loading context...
							</div>
						)}

						{contextError && (
							<div className="context-info error">
								<p>⚠️ {contextError}</p>
								<button type="button" onClick={() => window.location.reload()}>
									Retry
								</button>
							</div>
						)}

						{context && (
							<div className="context-info">
								<div className="context-header">
									<h3>📊 Dataset Context</h3>
									<button
										type="button"
										className="context-refresh-btn"
										onClick={refreshContext}
										disabled={contextLoading}
										title="Refresh context"
									>
										🔄
									</button>
								</div>
								<div className="context-details">
									<p>
										<strong>Dataset Size:</strong>{" "}
										{context.dataset_size.toLocaleString()} vectors
									</p>
									<p>
										<strong>Query Vectors:</strong>{" "}
										{context.num_query_vectors.toLocaleString()} available
									</p>
									<p>
										<strong>Valid Query Range:</strong> 0 -{" "}
										{context.max_dataset_index.toLocaleString()}
									</p>
								</div>
							</div>
						)}

						<div className="form-group">
							<label htmlFor="query-index">Query Index</label>
							<div className="input-group">
								<input
									id="query-index"
									type="number"
									value={queryIndex}
									onChange={(e) => setQueryIndex(parseInt(e.target.value) || 0)}
									placeholder="Enter query index"
									disabled={loading || contextLoading}
									className={!isQueryIndexValid() ? "error" : ""}
									min="0"
									max={context?.max_dataset_index || undefined}
								/>
								{context && (
									<button
										type="button"
										className="random-btn"
										onClick={generateRandomQuery}
										disabled={loading || contextLoading}
										title="Generate random query index"
									>
										🎲
									</button>
								)}
							</div>
							{getQueryIndexError() && (
								<div className="field-error">{getQueryIndexError()}</div>
							)}
							{context && isQueryIndexValid() && (
								<div className="field-hint">
									Valid range: 0 - {context.max_dataset_index.toLocaleString()}
								</div>
							)}
						</div>
						<div className="form-group">
							<label htmlFor="k-value">K Value</label>
							<input
								id="k-value"
								type="number"
								value={k}
								onChange={(e) => setK(parseInt(e.target.value) || 1)}
								placeholder="Enter k value"
								disabled={loading}
							/>
						</div>
						<div className="button-group">
							<button
								type="button"
								onClick={submitCompareRequest}
								disabled={
									loading ||
									!queryIndex ||
									!k ||
									!isQueryIndexValid() ||
									contextLoading
								}
							>
								{loading ? (
									<>
										<div className="spinner"></div>
										Comparing...
									</>
								) : (
									"Compare Algorithms"
								)}
							</button>
							<button type="button" onClick={resetForm} disabled={loading}>
								Reset
							</button>
						</div>
					</div>

					{jobId && loading && (
						<div className="status-section">
							<h3>Processing</h3>
							<div className="status-content">
								<div className="pulse-loader"></div>
								<p>
									Job ID: <code>{jobId}</code>
								</p>
								<p>Checking status every 1 second...</p>
							</div>
						</div>
					)}

					{error && (
						<div className="error-section">
							<h3>Error</h3>
							<p>{error}</p>
						</div>
					)}
				</div>

				<div className="results-container">
					{result && (
						<div className="results-section">
							<div className="results-header">
								<h2>Results</h2>
								<div className="view-controls">
									<button
										type="button"
										className={viewMode === "cards" ? "active" : ""}
										onClick={() => setViewMode("cards")}
									>
										Cards
									</button>
									<button
										type="button"
										className={viewMode === "comparison" ? "active" : ""}
										onClick={() => setViewMode("comparison")}
									>
										Comparison
									</button>
									<button
										type="button"
										className={viewMode === "detailed" ? "active" : ""}
										onClick={() => setViewMode("detailed")}
									>
										Detailed
									</button>
								</div>
							</div>

							<div className="result-summary">
								<div className="summary-item">
									<span className="summary-label">Query Index</span>
									<span className="summary-value">{queryIndex}</span>
								</div>
								<div className="summary-item">
									<span className="summary-label">K Value</span>
									<span className="summary-value">{k}</span>
								</div>
								<div className="summary-item">
									<span className="summary-label">Algorithms</span>
									<span className="summary-value">{result.results.length}</span>
								</div>
								<div className="summary-item">
									<span className="summary-label">Success Rate</span>
									<span className="summary-value">
										{(
											(result.results.filter((r) => r.success).length /
												result.results.length) *
											100
										).toFixed(0)}
										%
									</span>
								</div>
							</div>

							<div className="results-content">
								{viewMode === "cards" && (
									<div className="algorithms-grid">
										{result.results.map((algorithm) => (
											<div
												key={algorithm.algorithm_name}
												className="algorithm-card"
											>
												<h3>{algorithm.algorithm_name}</h3>
												<div className="algorithm-stats">
													<p>
														<strong>Precision:</strong>{" "}
														{(algorithm.precision * 100).toFixed(2)}%
													</p>
													<p>
														<strong>Execution Time:</strong>{" "}
														{algorithm.execution_time_ms.toFixed(4)} ms
													</p>
													<p>
														<strong>Status:</strong>{" "}
														{algorithm.success ? "✅ Success" : "❌ Failed"}
													</p>
												</div>

												<div className="distances-section">
													<h4>
														Top {algorithm.distances_and_matches.length} Results
													</h4>
													<div className="distances-grid">
														{algorithm.distances_and_matches.map(
															(match, matchIndex) => (
																<div
																	key={`${algorithm.algorithm_name}-${matchIndex}`}
																	className={`distance-item ${match.is_match ? "match" : "no-match"}`}
																>
																	<span className="distance">
																		{match.distance_squared.toFixed(0)}
																	</span>
																	<span className="match-status">
																		{match.is_match ? "✓" : "✗"}
																	</span>
																</div>
															),
														)}
													</div>
												</div>
											</div>
										))}
									</div>
								)}

								{viewMode === "comparison" && renderComparisonTable()}
								{viewMode === "detailed" && renderDetailedView()}
							</div>
						</div>
					)}
				</div>
			</div>
		</div>
	);
}

export default App;
