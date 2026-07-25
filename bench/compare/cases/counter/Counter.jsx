export default function Counter({ label = "Counter" }) {
  const [count, setCount] = React.useState(0);
  return (
    <div className="flex flex-col gap-4 p-6 items-center">
      <h1 className="text-2xl font-bold">{label}</h1>
      <p className="text-lg text-gray-500">Count: {count}</p>
      <div className="flex gap-2 items-center">
        <button className="btn outline" onClick={() => setCount(count - 1)}>-</button>
        <span className="text-4xl font-bold">{count}</span>
        <button className="btn primary" onClick={() => setCount(count + 1)}>+</button>
      </div>
      <button className="btn ghost" onClick={() => setCount(0)}>Reset</button>
    </div>
  );
}
